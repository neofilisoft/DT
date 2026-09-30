using System;
using System.IO;

namespace LacrimaBuild
{
    public static class Packager
    {
        public static int RunPackage(string engineRoot, string platform)
        {
            Console.WriteLine("========================================");
            Console.WriteLine($" LacrimaBuild: Packaging for {platform}");
            Console.WriteLine("========================================");

            string stagingDir = Path.Combine(engineRoot, "dist", platform);
            if (Directory.Exists(stagingDir))
            {
                Directory.Delete(stagingDir, true);
            }
            Directory.CreateDirectory(stagingDir);

            // 1. Build the game in Release
            Console.WriteLine("1. Building Release Binaries...");
            int buildResult = Builder.RunBuild(engineRoot, "Release", "-DDT_BUILD_GAME=ON");
            if (buildResult != 0) return buildResult;

            // 2. Cook Assets
            Console.WriteLine("2. Cooking Assets...");
            string assetDir = Path.Combine(engineRoot, "assets");
            int cookResult = CookerWrapper.RunCook(engineRoot, assetDir);
            if (cookResult != 0) return cookResult;

            // 3. Copy Game Executable
            Console.WriteLine("3. Assembling Package...");
            string gameExe = Path.Combine(engineRoot, "build", "source", "game", "Release", "Domaintic.exe");
            if (!File.Exists(gameExe))
                gameExe = Path.Combine(engineRoot, "build", "source", "game", "Domaintic.exe");

            if (!File.Exists(gameExe))
            {
                Console.WriteLine($"Error: Could not find game executable at {gameExe}");
                return 1;
            }

            File.Copy(gameExe, Path.Combine(stagingDir, Path.GetFileName(gameExe)));

            // 4. Copy Cooked Assets (only .dtas files)
            string targetAssetDir = Path.Combine(stagingDir, "assets");
            Directory.CreateDirectory(targetAssetDir);
            foreach (string file in Directory.GetFiles(assetDir, "*.dtas", SearchOption.AllDirectories))
            {
                string relPath = Path.GetRelativePath(assetDir, file);
                string destPath = Path.Combine(targetAssetDir, relPath);
                Directory.CreateDirectory(Path.GetDirectoryName(destPath));
                File.Copy(file, destPath, true);
            }

            Console.WriteLine("========================================");
            Console.WriteLine($" Successfully packaged to: {stagingDir}");
            return 0;
        }
    }
}

