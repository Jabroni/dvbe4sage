using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.ServiceProcess;
using System.Threading;
using System.Windows.Forms;

namespace dvbe4sagesvc
{
    public partial class ServiceMain : ServiceBase
    {
        [DllImport("encoder")]
        static extern void createEncoder(IntPtr hInstance, IntPtr hWnd, IntPtr hParentMenu);
        [DllImport("encoder")]
        static extern void deleteEncoder();
        [DllImport("encoder")]
        static extern void waitForFullInitialization();

        Thread workerThread;
        HiddenForm form;
        EventWaitHandle waitHandle;
        ApplicationContext context;

        public static void doWork(object param)
        {
            // Get the service object
            ServiceMain myService = (ServiceMain)param;

            // Our hidden form (we need it for getting a window handle)
            myService.form = new HiddenForm();

            // Hide the form
            myService.form.Hide();

            // Create the encoder
            createEncoder(Marshal.GetHINSTANCE(typeof(ServiceMain).Module), myService.form.Handle, (IntPtr)0);

            // Signal wait handle (the encoder object has been created)
            myService.waitHandle.Set();

            // Create the application context object
            myService.context = new ApplicationContext();

            // Run the main loop, needs to be finished in the service OnStop
            Application.Run(myService.context);
        }

        public ServiceMain()
        {
            // Generated code
            InitializeComponent();

            // Set current directory according to the assembly path
            Directory.SetCurrentDirectory(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location));

            // Initialize event wait handle
            waitHandle = new EventWaitHandle(false, EventResetMode.AutoReset);
        }

        protected override void OnStart(string[] args)
        {
            // Create the worker thred
            workerThread = new Thread(new ParameterizedThreadStart(doWork));

            // And start it
            workerThread.Start(this);

            // Wait till the encoder object is created
            waitHandle.WaitOne();

            // Now, wait for full encoder initialization
            waitForFullInitialization();
        }

        protected override void OnStop()
        {
            // Delete the encoder
            deleteEncoder();

            // End the worker thread
            context.ExitThread();
        }
    }
}
