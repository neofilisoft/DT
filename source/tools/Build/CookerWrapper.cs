using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace LacrimaBuild
{
    public static class CookerWrapper
    {
        public static int RunCook(string engineRoot, string assetDir)
        {
            Console.WriteLine("========================================");
            Console.WriteLine($" LacrimaBuild: Cooking Assets in {assetDir}");
            Console.WriteLine("========================================");

            // Locate the C++ cooker executable
            string cookerExePath = Path.Combine(engineRoot, "build", "source", "tools", "cooker", "DTCooker.exe");
            
            // Fallback for Debug/Release folders if CMake generated them
            if (!File.Exists(cookerExePath))
            {
                cookerExePath = Path.Combine(engineRoot, "build", "source", "tools", "cooker", "Debug", "DTCooker.exe");
            }

            if (!File.Exists(cookerExePath))
            {
                Console.WriteLine($"Error: Cannot find DTCooker.exe at {cookerExePath}");
                Console.WriteLine("Please run 'LacrimaBuild build' first.");
                return 1;
            }

            if (!Directory.Exists(assetDir))
            {
                Console.WriteLine($"Error: Asset directory {assetDir} does not exist.");
                return 1;
            }

            string[] files = Directory.GetFiles(assetDir, "*.*", SearchOption.AllDirectories)
                .Where(f => f.EndsWith(".png", StringComparison.OrdinalIgnoreCase) || 
                            f.EndsWith(".jpg", StringComparison.OrdinalIgnoreCase) ||
                            f.EndsWith(".glb", StringComparison.OrdinalIgnoreCase) ||
                            f.EndsWith(".gltf", StringComparison.OrdinalIgnoreCase))
                .ToArray();

            if (files.Length == 0)
            {
                Console.WriteLine("No raw assets found to cook.");
                return 0;
            }

            Console.WriteLine($"Found {files.Length} assets. Cooking across multiple threads...");

            int successCount = 0;
            int failCount = 0;

            Parallel.ForEach(files, new ParallelOptions { MaxDegreeOfParallelism = Environment.ProcessorCount }, file =>
            {
                string outputFile = file + ".dtas"; // Output path for cooked asset
                string extension = Path.GetExtension(file);
                string type = extension.Equals(".glb", StringComparison.OrdinalIgnoreCase) ||
                              extension.Equals(".gltf", StringComparison.OrdinalIgnoreCase)
                    ? "mesh"
                    : "texture";

                var processInfo = new ProcessStartInfo
                {
                    FileName = cookerExePath,
                    Arguments = $"-i \"{file}\" -o \"{outputFile}\" -t {type}",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };

                using (var process = Process.Start(processInfo))
                {
                    process?.WaitForExit();
                    if (process?.ExitCode == 0)
                    {
                        System.Threading.Interlocked.Increment(ref successCount);
                        Console.WriteLine($"[OK] {Path.GetFileName(file)}");
                    }
                    else
                    {
                        System.Threading.Interlocked.Increment(ref failCount);
                        string error = process?.StandardError.ReadToEnd() ?? "Unknown error";
                        Console.WriteLine($"[FAIL] {Path.GetFileName(file)} - {error}");
                    }
                }
            });

            Console.WriteLine("========================================");
            Console.WriteLine($" Cooking complete. Success: {successCount}, Failed: {failCount}");
            return failCount > 0 ? 1 : 0;
        }
    }
}

