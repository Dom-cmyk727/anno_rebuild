import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
// Writes the decompiled C of every function to args[0].
public class DecompileAll extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        PrintWriter out = new PrintWriter(new FileWriter(args[0]));
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            out.println("// ===== " + f.getName() + " @ " + f.getEntryPoint());
            out.println(r.getDecompiledFunction() == null ? "// " + r.getErrorMessage() : r.getDecompiledFunction().getC());
        }
        out.close();
    }
}
