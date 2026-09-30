using System;
using System.Diagnostics;
using System.IO;

namespace LacrimaBuild
{
    public static class Builder
    {
        public static int RunBuild(string engineRoot, string config = "Debug", string extraCmakeArgs = "")
        {
            Console.WriteLine("========================================");
            Console.WriteLine($" LacrimaBuild: Building Engine ({config})");
            Console.WriteLine("========================================");

            string buildDir = Path.Combine(engineRoot, "build");
            string cmakeCache = Path.Combine(buildDir, "CMakeCache.txt");
            if (!File.Exists(cmakeCache))
            {
                Console.WriteLine("Generating CMake configuration...");
                if (!Directory.Exists(buildDir))
                    Directory.CreateDirectory(buildDir);
                    
                int genCode = RunCommand("cmake", $".. -DCMAKE_BUILD_TYPE={config} {extraCmakeArgs}", buildDir);
                if (genCode != 0) return genCode;
            }

            Console.WriteLine("Compiling binaries...");
            return RunCommand("cmake", $"--build . --config {config}", buildDir);
        }

        private static int RunCommand(string fileName, string arguments, string workingDirectory)
        {
            var processInfo = new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                WorkingDirectory = workingDirectory,
                UseShellExecute = false,
                RedirectStandardOutput = false,
                RedirectStandardError = false,
            };

            using (var process = Process.Start(processInfo))
            {
                if (process == null)
                {
                    Console.WriteLine($"Failed to start process: {fileName}");
                    return 1;
                }

                process.WaitForExit();
                return process.ExitCode;
            }
        }
    }
}

