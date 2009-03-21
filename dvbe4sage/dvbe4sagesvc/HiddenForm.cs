using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace dvbe4sagesvc
{
    public partial class HiddenForm : Form
    {
        [DllImport("encoder")]
        static extern IntPtr WindowProc(int message, IntPtr wParam, IntPtr lParam);

        public HiddenForm()
        {
            InitializeComponent();
        }
        protected override void WndProc(ref Message m)
        {
            IntPtr res = WindowProc(m.Msg, m.WParam, m.LParam);
            m.Result = res;
            base.WndProc(ref m);
        }
    }
}
