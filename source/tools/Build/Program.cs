using System;
using System.IO;

namespace LacrimaBuild
{
    class Program
    {
        static int Main(string[] args)
        {
            if (args.Length == 0)
            {
                PrintUsage();
                return 1;
            }

            string command = args[0].ToLowerInvariant();
            
            // Find root by walking up until CMakeLists.txt is found
            string engineRoot = AppContext.BaseDirectory;
            while (!string.IsNullOrEmpty(engineRoot) && !File.Exists(Path.Combine(engineRoot, "CMakeLists.txt")))
            {
                engineRoot = Path.GetDirectoryName(engineRoot);
            }

            if (string.IsNullOrEmpty(engineRoot))
            {
                Console.WriteLine("Error: Could not find engine root (CMakeLists.txt).");
                return 1;
            }

            try
            {
                switch (command)
                {
                    case "build":
                        string config = "Debug";
                        if (args.Length > 1) config = args[1];
                        return Builder.RunBuild(engineRoot, config);

                    case "cook":
                        string assetDir = Path.Combine(engineRoot, "assets");
                        if (args.Length > 1) assetDir = Path.GetFullPath(args[1]);
                        return CookerWrapper.RunCook(engineRoot, assetDir);

                    case "package":
                        string platform = "win-x64";
                        if (args.Length > 1) platform = args[1];
                        return Packager.RunPackage(engineRoot, platform);

                    case "help":
                        PrintUsage();
                        return 0;

                    default:
                        Console.WriteLine($"Unknown command: {command}");
                        PrintUsage();
                        return 1;
                }
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"FATAL ERROR: {ex.Message}");
                Console.WriteLine(ex.StackTrace);
                Console.ResetColor();
                return 1;
            }
        }

        static void PrintUsage()
        {
            Console.WriteLine("LacrimaBuild - Master Orchestrator for DT Engine");
            Console.WriteLine("Usage:");
            Console.WriteLine("  LacrimaBuild build [Debug|Release]   - Builds the engine and game binaries via CMake");
            Console.WriteLine("  LacrimaBuild cook [AssetDir]         - Cooks assets using DTCooker");
            Console.WriteLine("  LacrimaBuild package [Platform]      - Cooks assets, builds Release, and packages the game into dist/");
        }
    }
}

