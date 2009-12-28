using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace dvbe4sagesvc
{
    public partial class HiddenForm : Form
    {
        [DllImport("encoder")]
        static extern IntPtr encoderWindowProc(int message, IntPtr wParam, IntPtr lParam);

        public HiddenForm()
        {
            InitializeComponent();
        }
        protected override void WndProc(ref Message m)
        {
            IntPtr res = encoderWindowProc(m.Msg, m.WParam, m.LParam);
            m.Result = res;
            base.WndProc(ref m);
        }
    }
}
