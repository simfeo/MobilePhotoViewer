using Xamarin.Forms;
using Xamarin.Forms.Platform.WPF;

namespace MobilePhotoViewer.WPF
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : FormsApplicationPage
    {
        public MainWindow()
        {
            InitializeComponent();

            Forms.Init(); // Xamarin Form Call
            LoadApplication(new App3.App());
        }
    }
}
