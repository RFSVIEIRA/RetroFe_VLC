using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;


namespace RetroFE_Start
{
    class Program
    {
        static void Main(string[] args)
        {
            controlRetrofe();
        }

        public static void controlRetrofe()
        {
            string filePath = @"core\retrofe.exe";
            if (File.Exists(filePath))
            {

                //start retrofe
                System.Diagnostics.Process retrofeProcess = new Process();
                retrofeProcess.StartInfo.FileName = @"core\retrofe.exe";
                _ = retrofeProcess.Start();


                // waiting on retrofe to close
                retrofeProcess.WaitForExit();
                _ = retrofeProcess.ExitCode;
                retrofeProcess.Close();
            }
        }
    }
}
