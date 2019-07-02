namespace MobilePhotoViewer.UWP
{
    public sealed partial class MainPage
    {
        public MainPage()
        {
            this.InitializeComponent();

            LoadApplication(new MobilePhotoViewer.App());
        }
    }
}
