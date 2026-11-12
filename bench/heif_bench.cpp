// Decode benchmark: times the viewer's current image paths against the
// alternatives on a directory of test images (see fetch_data.sh).
//
//   heif_bench <set_dir> [--reps N] [--only SUBSTR] [--csv PATH]
//              [--no-grid] [--grid-threads N]

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMap>
#include <QThreadPool>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

#include <sys/resource.h>

#include <libheif/heif.h>

#include "formats/heifmanager.h"
#include "formats/heifpreview.h"
#include "formats/jpegrepair.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

namespace
{
    constexpr int kThumb = 128 * 3;   // ThumbnailModel::kThumb

    struct Outcome
    {
        bool    ok = false;
        int     w  = 0;
        int     h  = 0;
        QString note;
    };

    Outcome fromImage(const QImage& img, const QString& note = {})
    {
        return {!img.isNull(), img.width(), img.height(), note};
    }

    bool isHeif(const QString& path)
    {
        const QString s = QFileInfo(path).suffix().toLower();
        return s == "heic" || s == "heif";
    }

    // ---- What the app does today -------------------------------------------

    // MainWindow reads each opened file once and hands the bytes to everything else.
    QByteArray readFile(const QString& p)
    {
        QFile f(p);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    // MainWindow's full HEIC decode, now on a worker; the copy stands in for QPixmap::fromImage.
    Outcome appOpenHeif(const QString& p)
    {
        const HeifManager m(readFile(p));
        return fromImage(m.image().copy());
    }

    // What MainWindow shows on the UI thread while that decode runs.
    Outcome appOpenPreviewHeif(const QString& p)
    {
        return fromImage(HeifPreview::thumbnail(readFile(p), 1024, false));
    }

    // ThumbnailModel filling one HEIC grid cell.
    Outcome appThumbHeif(const QString& p)
    {
        return fromImage(HeifPreview::thumbnail(p, kThumb));
    }

    // MetadataPanel on grid selection, on the UI thread.
    Outcome appInfoPanelHeif(const QString& p)
    {
        const QByteArray data = readFile(p);
        const QImage     img  = HeifPreview::thumbnail(data, 512);
        const QSize      dims = HeifPreview::size(data);
        return {!img.isNull() && !dims.isEmpty(), dims.width(), dims.height(), {}};
    }

    Outcome appOpenRaster(const QString& p)
    {
        return fromImage(JpegRepair::read(p, true));
    }

    Outcome appThumbRaster(const QString& p)
    {
        return fromImage(JpegRepair::read(p, true, kThumb));
    }

    // ---- libheif used directly ---------------------------------------------

    using CtxPtr = std::unique_ptr<heif_context, decltype(&heif_context_free)>;

    CtxPtr openHeif(const QString& p, int threads)
    {
        CtxPtr ctx(heif_context_alloc(), &heif_context_free);
        if (threads > 0)
            heif_context_set_max_decoding_threads(ctx.get(), threads);
        if (heif_context_read_from_file(ctx.get(), QFile::encodeName(p).constData(), nullptr).code != heif_error_Ok)
            ctx.reset();
        return ctx;
    }

    // The QImage takes ownership of the decoded heif_image, so no pixel copy.
    // decoderId picks a libheif decoder plugin ("libde265", "ffmpeg"); null means the default.
    QImage decodeHandle(heif_image_handle* h, const char* decoderId = nullptr)
    {
        std::unique_ptr<heif_decoding_options, decltype(&heif_decoding_options_free)> opts(
            heif_decoding_options_alloc(), &heif_decoding_options_free);
        opts->decoder_id = decoderId;
        heif_image* img  = nullptr;
        if (heif_decode_image(h, &img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, opts.get()).code != heif_error_Ok)
            return {};
        int            stride = 0;
        const uint8_t* plane  = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
        if (!plane)
        {
            heif_image_release(img);
            return {};
        }
        return QImage(plane, heif_image_get_width(img, heif_channel_interleaved),
                      heif_image_get_height(img, heif_channel_interleaved), stride, QImage::Format_RGBA8888,
                      [](void* i) { heif_image_release(static_cast<heif_image*>(i)); }, img);
    }

    Outcome heifPrimary(const QString& p, int threads, const char* decoderId = nullptr)
    {
        CtxPtr ctx = openHeif(p, threads);
        if (!ctx)
            return {};
        heif_image_handle* h = nullptr;
        if (heif_context_get_primary_image_handle(ctx.get(), &h).code != heif_error_Ok)
            return {};
        const QImage img = decodeHandle(h, decoderId);
        heif_image_handle_release(h);
        return fromImage(img);
    }

    Outcome heifEmbeddedThumb(const QString& p)
    {
        CtxPtr ctx = openHeif(p, 0);
        if (!ctx)
            return {};
        heif_image_handle* h = nullptr;
        if (heif_context_get_primary_image_handle(ctx.get(), &h).code != heif_error_Ok)
            return {};

        heif_image_handle* thumb = nullptr;
        heif_item_id       id    = 0;
        if (heif_image_handle_get_list_of_thumbnail_IDs(h, &id, 1) == 1)
            heif_image_handle_get_thumbnail(h, id, &thumb);

        QImage img = decodeHandle(thumb ? thumb : h);
        if (thumb)
            heif_image_handle_release(thumb);
        heif_image_handle_release(h);
        if (img.width() > kThumb || img.height() > kThumb)
            img = img.scaled(kThumb, kThumb, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return fromImage(img, thumb ? "embedded" : "no_thumb_full_decode");
    }

    // ---- macOS ImageIO ------------------------------------------------------

#ifdef __APPLE__
    struct CFRelease_
    {
        void operator()(CFTypeRef r) const
        {
            if (r)
                CFRelease(r);
        }
    };
    template <typename T>
    using CF = std::unique_ptr<std::remove_pointer_t<T>, CFRelease_>;

    CF<CGImageSourceRef> openSource(const QString& p)
    {
        const QByteArray path = QFile::encodeName(p);
        CF<CFURLRef>     url(CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8*>(path.constData()), path.size(), false));
        return CF<CGImageSourceRef>(url ? CGImageSourceCreateWithURL(url.get(), nullptr) : nullptr);
    }

    // CGImages decode lazily; copying the pixels forces the decode the app would pay for.
    Outcome realise(CGImageRef img)
    {
        if (!img)
            return {};
        CF<CFDataRef> px(CGDataProviderCopyData(CGImageGetDataProvider(img)));
        const Outcome o{px != nullptr, int(CGImageGetWidth(img)), int(CGImageGetHeight(img)), {}};
        CGImageRelease(img);
        return o;
    }

    CF<CFDictionaryRef> makeDict(const void** keys, const void** values, CFIndex n)
    {
        return CF<CFDictionaryRef>(CFDictionaryCreate(nullptr, keys, values, n,
                                                      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    Outcome imageioOpen(const QString& p)
    {
        CF<CGImageSourceRef> src = openSource(p);
        if (!src)
            return {};
        const void*         k[] = {kCGImageSourceShouldCacheImmediately};
        const void*         v[] = {kCFBooleanTrue};
        CF<CFDictionaryRef> opts = makeDict(k, v, 1);
        return realise(CGImageSourceCreateImageAtIndex(src.get(), 0, opts.get()));
    }

    Outcome imageioThumb(const QString& p)
    {
        CF<CGImageSourceRef> src = openSource(p);
        if (!src)
            return {};
        int                 maxPx = kThumb;
        CF<CFNumberRef>     n(CFNumberCreate(nullptr, kCFNumberIntType, &maxPx));
        const void*         k[] = {kCGImageSourceCreateThumbnailFromImageIfAbsent, kCGImageSourceCreateThumbnailWithTransform,
                                   kCGImageSourceThumbnailMaxPixelSize, kCGImageSourceShouldCacheImmediately};
        const void*         v[] = {kCFBooleanTrue, kCFBooleanTrue, n.get(), kCFBooleanTrue};
        CF<CFDictionaryRef> opts = makeDict(k, v, 4);
        return realise(CGImageSourceCreateThumbnailAtIndex(src.get(), 0, opts.get()));
    }
#endif

    // ---- Harness ------------------------------------------------------------

    enum class Applies
    {
        Heif,
        Raster,
        Any
    };

    struct Scenario
    {
        QString                               name;
        Applies                               applies;
        bool                                  isThumb;
        std::function<Outcome(const QString&)> run;

        bool appliesTo(const QString& path) const
        {
            return applies == Applies::Any || (applies == Applies::Heif) == isHeif(path);
        }
    };

    std::vector<Scenario> allScenarios()
    {
        return {
            {"app_open", Applies::Heif, false, appOpenHeif},
            {"app_open_preview", Applies::Heif, false, appOpenPreviewHeif},
            {"app_info_panel", Applies::Heif, false, appInfoPanelHeif},
            {"app_thumb", Applies::Heif, true, appThumbHeif},
            {"app_open", Applies::Raster, false, appOpenRaster},
            {"app_thumb", Applies::Raster, true, appThumbRaster},
            {"heif_primary", Applies::Heif, false, [](const QString& p) { return heifPrimary(p, 0); }},
            {"heif_primary_t1", Applies::Heif, false, [](const QString& p) { return heifPrimary(p, 1); }},
            {"heif_primary_ffmpeg", Applies::Heif, false, [](const QString& p) { return heifPrimary(p, 0, "ffmpeg"); }},
            {"heif_embedded_thumb", Applies::Heif, true, heifEmbeddedThumb},
#ifdef __APPLE__
            {"imageio_open", Applies::Any, false, imageioOpen},
            {"imageio_thumb", Applies::Any, true, imageioThumb},
#endif
        };
    }

    double percentile(std::vector<double> v, double q)
    {
        if (v.empty())
            return 0;
        std::sort(v.begin(), v.end());
        return v[std::min(v.size() - 1, size_t(q * double(v.size() - 1) + 0.5))];
    }

    // HeifManager logs per image; drop it so stderr stays readable.
    void silence(QtMsgType, const QMessageLogContext&, const QString&) {}

    QByteArray imageHash(const QImage& img)
    {
        if (img.isNull())
            return "decode-failed";
        QCryptographicHash md5(QCryptographicHash::Md5);
        for (int y = 0; y < img.height(); ++y)
            md5.addData(QByteArrayView(reinterpret_cast<const char*>(img.constScanLine(y)), qsizetype(img.width()) * 4));
        return md5.result().toHex();
    }

    // Pixel checksum of a full decode, for proving a SIMD path matches the scalar one.
    QByteArray pixelHash(const QString& p, const char* decoderId)
    {
        CtxPtr ctx = openHeif(p, 0);
        if (!ctx)
            return "open-failed";
        heif_image_handle* h = nullptr;
        if (heif_context_get_primary_image_handle(ctx.get(), &h).code != heif_error_Ok)
            return "no-primary";
        const QImage img = decodeHandle(h, decoderId);
        heif_image_handle_release(h);
        return imageHash(img);
    }
}   // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2)
    {
        std::fprintf(stderr, "usage: heif_bench <set_dir> [--reps N] [--only SUBSTR] [--csv PATH] [--no-grid] [--grid-threads N]\n");
        return 2;
    }

    const QString setDir      = args[1];
    int           reps        = 3;
    int           gridThreads = 4;   // ThumbnailModel's pool size
    bool          grid        = true;
    bool          hashOnly    = false;
    QString       only;
    QString       csvPath = "bench/results/heif_bench.csv";
    for (int i = 2; i < args.size(); ++i)
    {
        const QString& a = args[i];
        if (a == "--reps" && i + 1 < args.size())
            reps = args[++i].toInt();
        else if (a == "--only" && i + 1 < args.size())
            only = args[++i];
        else if (a == "--csv" && i + 1 < args.size())
            csvPath = args[++i];
        else if (a == "--grid-threads" && i + 1 < args.size())
            gridThreads = args[++i].toInt();
        else if (a == "--no-grid")
            grid = false;
        else if (a == "--hash")
            hashOnly = true;
    }

    QStringList files;
    for (QDirIterator it(setDir, QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
    {
        const QString p = it.next();
        const QString s = QFileInfo(p).suffix().toLower();
        if (s == "heic" || s == "heif" || s == "png" || s == "jpg" || s == "jpeg")
            files << p;
    }
    files.sort();
    if (files.isEmpty())
    {
        std::fprintf(stderr, "no images under %s\n", qPrintable(setDir));
        return 1;
    }

    if (hashOnly)
    {
        qInstallMessageHandler(silence);
        const QDir root(QFileInfo(setDir).absoluteFilePath());
        for (const QString& p : files)
        {
            if (!isHeif(p))
                continue;
            const QString rel = root.relativeFilePath(QFileInfo(p).absoluteFilePath());
            for (const char* dec : {"libde265", "ffmpeg"})
                std::printf("%s %s %s\n", dec, qPrintable(rel), pixelHash(p, dec).constData());
            std::printf("app %s %s\n", qPrintable(rel), imageHash(HeifManager(readFile(p)).image()).constData());
            std::fflush(stdout);
        }
        return 0;
    }

    std::vector<Scenario> scenarios;
    for (Scenario& s : allScenarios())
        if (only.isEmpty() || s.name.contains(only))
            scenarios.push_back(s);

    QDir().mkpath(QFileInfo(csvPath).absolutePath());
    QFile csv(csvPath);
    if (!csv.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(csvPath));
        return 1;
    }
    csv.write("group,file,scenario,rep,ms,ok,width,height,note\n");

    qInstallMessageHandler(silence);
    std::printf("heif_bench: %lld files, %d reps, csv %s\n", qlonglong(files.size()), reps, qPrintable(csvPath));
    std::fflush(stdout);

    const QDir                          root(QFileInfo(setDir).absoluteFilePath());
    QMap<QString, std::vector<double>>  best;   // "group | scenario" -> per-file best ms
    QMap<QString, int>                  failures;
    QMap<QString, QStringList>          groupFiles;

    for (qsizetype i = 0; i < files.size(); ++i)
    {
        const QString& path  = files[i];
        const QString  rel   = root.relativeFilePath(QFileInfo(path).absoluteFilePath());
        const QString  group = rel.contains('/') ? rel.section('/', 0, 0) : QString(".");
        groupFiles[group] << path;
        std::printf("[single] %lld/%lld %s\n", qlonglong(i + 1), qlonglong(files.size()), qPrintable(rel));
        std::fflush(stdout);

        // Pull the file into the page cache so disk speed does not skew the first scenario.
        QFile warm(path);
        if (warm.open(QIODevice::ReadOnly))
            warm.readAll();

        for (const Scenario& s : scenarios)
        {
            if (!s.appliesTo(path))
                continue;
            double fileBest = -1;
            bool   ok       = true;
            for (int r = 0; r < reps; ++r)
            {
                QElapsedTimer t;
                t.start();
                const Outcome o  = s.run(path);
                const double  ms = double(t.nsecsElapsed()) / 1e6;
                ok               = ok && o.ok;
                fileBest         = fileBest < 0 ? ms : std::min(fileBest, ms);
                csv.write(QString("%1,%2,%3,%4,%5,%6,%7,%8,%9\n")
                              .arg(group, rel, s.name)
                              .arg(r)
                              .arg(ms, 0, 'f', 3)
                              .arg(o.ok ? 1 : 0)
                              .arg(o.w)
                              .arg(o.h)
                              .arg(o.note)
                              .toUtf8());
            }
            const QString key = group + " | " + s.name;
            if (ok)
                best[key].push_back(fileBest);
            else
                failures[key]++;
        }
        csv.flush();
    }

    std::printf("\n=== single-image decode, per-file best of %d reps (ms) ===\n", reps);
    std::printf("%-18s %-20s %6s %9s %9s %9s %6s\n", "group", "scenario", "files", "median", "p90", "total", "fail");
    for (auto it = best.cbegin(); it != best.cend(); ++it)
    {
        const std::vector<double>& v = it.value();
        double                     total = 0;
        for (double d : v)
            total += d;
        const QStringList parts = it.key().split(" | ");
        std::printf("%-18s %-20s %6zu %9.1f %9.1f %9.0f %6d\n", qPrintable(parts[0]), qPrintable(parts[1]), v.size(),
                    percentile(v, 0.5), percentile(v, 0.9), total, failures.value(it.key()));
    }
    for (auto it = failures.cbegin(); it != failures.cend(); ++it)
        if (!best.contains(it.key()))
            std::printf("%-33s all %d files FAILED\n", qPrintable(it.key()), it.value());
    std::fflush(stdout);

    if (grid)
    {
        std::printf("\n=== grid fill: every file in a group through a %d-thread pool, like ThumbnailModel ===\n", gridThreads);
        std::printf("%-18s %-20s %6s %10s %9s\n", "group", "scenario", "files", "wall ms", "files/s");
        for (auto g = groupFiles.cbegin(); g != groupFiles.cend(); ++g)
        {
            for (const Scenario& s : scenarios)
            {
                if (!s.isThumb || !s.appliesTo(g.value().first()))
                    continue;
                QThreadPool pool;
                pool.setMaxThreadCount(gridThreads);
                QElapsedTimer t;
                t.start();
                for (const QString& p : g.value())
                    pool.start([&s, p]() { s.run(p); });
                pool.waitForDone();
                const double ms = double(t.nsecsElapsed()) / 1e6;
                std::printf("%-18s %-20s %6lld %10.0f %9.1f\n", qPrintable(g.key()), qPrintable(s.name),
                            qlonglong(g.value().size()), ms, 1000.0 * double(g.value().size()) / ms);
                std::fflush(stdout);
                csv.write(QString("%1,*,grid:%2,0,%3,1,0,0,threads=%4\n").arg(g.key(), s.name).arg(ms, 0, 'f', 1).arg(gridThreads).toUtf8());
            }
        }
    }

    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    const double peakMb = double(ru.ru_maxrss) / (1024.0 * 1024.0);   // bytes on macOS
#else
    const double peakMb = double(ru.ru_maxrss) / 1024.0;   // KiB on Linux
#endif
    std::printf("\npeak RSS: %.0f MB (whole run; use --only to isolate one scenario)\n", peakMb);
    return 0;
}
