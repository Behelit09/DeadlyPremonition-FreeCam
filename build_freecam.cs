using System;
using System.Diagnostics;
using System.IO;

class Program {
    static int RunZig(string zigExe, string cacheDir, string args, out string stdout, out string stderr) {
        var psi = new ProcessStartInfo {
            FileName = zigExe,
            Arguments = args,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };
        psi.EnvironmentVariables["ZIG_GLOBAL_CACHE_DIR"] = cacheDir;
        psi.EnvironmentVariables["ZIG_LOCAL_CACHE_DIR"] = cacheDir;

        var proc = Process.Start(psi);
        stdout = proc.StandardOutput.ReadToEnd();
        stderr = proc.StandardError.ReadToEnd();
        proc.WaitForExit();
        return proc.ExitCode;
    }

    static int Main(string[] args) {
        string root = Directory.GetCurrentDirectory();
        string zigExe = Path.Combine(root, ".zig\\zig.exe");
        string cacheDir = Path.Combine(root, ".zig-cache");
        string imguiDir = Path.Combine(root, "FreeCam\\external\\imgui");
        string backendsDir = Path.Combine(imguiDir, "backends");
        string includeWin = Path.Combine(root, ".zig\\lib\\libc\\include\\any-windows-any");
        string srcFile = Path.Combine(root, "FreeCam\\src\\FreeCam_unity.cpp");
        string outAsi = Path.Combine(root, "FreeCam\\build\\FreeCam.asi");

        if (!Directory.Exists(cacheDir)) Directory.CreateDirectory(cacheDir);
        string tmpDir = Path.Combine(cacheDir, "tmp");
        if (!Directory.Exists(tmpDir)) Directory.CreateDirectory(tmpDir);

        string outDir = Path.GetDirectoryName(outAsi);
        if (!Directory.Exists(outDir)) Directory.CreateDirectory(outDir);

        string hDir = Path.Combine(cacheDir, "h");
        if (Directory.Exists(hDir)) {
            foreach (var f in Directory.GetFiles(hDir)) {
                try { File.Delete(f); } catch { }
            }
        }

        string expectedObj = Path.Combine(tmpDir, "FreeCam_unity.obj");
        string directObj = Path.Combine(root, "FreeCam\\src\\FreeCam_unity.obj");

        if (File.Exists(expectedObj)) File.Delete(expectedObj);
        if (File.Exists(directObj)) File.Delete(directObj);

        // Step 1: Compile FreeCam_unity.cpp
        Console.WriteLine("[*] Compiling FreeCam_unity.cpp with Dear ImGui C++...");
        string compileArgs = string.Format(
            "c++ -target x86-windows -nostdlib -fno-exceptions -fno-rtti -fno-threadsafe-statics -I\"{0}\" -I\"{1}\" -I\"{2}\" -O2 -shared \"{3}\" -o \"{4}\"",
            includeWin, imguiDir, backendsDir, srcFile, outAsi
        );

        string stdout, stderr;
        RunZig(zigExe, cacheDir, compileArgs, out stdout, out stderr);

        string objToLink = null;
        if (File.Exists(directObj)) objToLink = directObj;
        else if (File.Exists(expectedObj)) objToLink = expectedObj;

        if (objToLink == null) {
            Console.WriteLine("[ERROR] Compilation failed:\n" + stderr);
            return 1;
        }

        Console.WriteLine("[*] Compiled object: " + objToLink);

        // Step 2: Link object into final FreeCam.asi
        Console.WriteLine("[*] Linking FreeCam.asi with DllMain entry point...");
        string linkArgs = string.Format(
            "cc -target x86-windows -nostdlib -shared -Wl,-e,DllMain@12 \"{0}\" -o \"{1}\" -lkernel32 -luser32 -lgdi32 -ld3d9",
            objToLink, outAsi
        );

        int linkRes = RunZig(zigExe, cacheDir, linkArgs, out stdout, out stderr);
        if (linkRes != 0) {
            Console.WriteLine("[ERROR] Linking failed:\n" + stderr);
            return linkRes;
        }

        Console.WriteLine("[*] Successfully built: " + outAsi);

        // Step 3: Deploy to scripts\
        string scriptsDir = Path.Combine(root, "scripts");
        if (Directory.Exists(scriptsDir)) {
            string destAsi = Path.Combine(scriptsDir, "FreeCam.asi");
            File.Copy(outAsi, destAsi, true);
            Console.WriteLine("[*] Deployed to: " + destAsi);

            string iniSrc = Path.Combine(root, "FreeCam\\config\\FreeCam.ini");
            string iniDest = Path.Combine(scriptsDir, "FreeCam.ini");
            if (File.Exists(iniSrc)) {
                File.Copy(iniSrc, iniDest, true);
                Console.WriteLine("[*] Deployed to: " + iniDest);
            }
        }

        return 0;
    }
}
