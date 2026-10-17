import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
public class Decompile extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        PrintWriter out = new PrintWriter(new FileWriter(args[0]));
        for (int i = 1; i < args.length; i++) {
            Address a = toAddr(args[i]);
            Function f = getFunctionContaining(a);
            if (f == null) { createFunction(a, null); f = getFunctionAt(a); }
            if (f == null) { out.println("// no function at " + args[i]); continue; }
            DecompileResults r = d.decompileFunction(f, 120, monitor);
            out.println("// ===== " + f.getName() + " @ " + f.getEntryPoint());
            out.println(r.getDecompiledFunction() == null ? r.getErrorMessage() : r.getDecompiledFunction().getC());
        }
        out.close();
    }
}
