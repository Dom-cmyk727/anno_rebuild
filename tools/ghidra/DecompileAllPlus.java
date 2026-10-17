import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import java.io.*;
// Like DecompileAll, but first creates functions at every pointer into an executable
// block found in non-executable initialised memory (vtables, jump tables), then decompiles all.
public class DecompileAllPlus extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Memory mem = currentProgram.getMemory();
        int created = 0;
        for (int pass = 0; pass < 2; pass++) {
          for (MemoryBlock b : mem.getBlocks()) {
            if (b.isExecute() || !b.isInitialized()) continue;
            Address a = b.getStart();
            while (a.compareTo(b.getEnd()) < 0) {
                try {
                    int v = mem.getInt(a);
                    Address t = toAddr(v & 0xffffffffL);
                    MemoryBlock tb = mem.getBlock(t);
                    if (tb != null && tb.isExecute() && getFunctionContaining(t) == null
                        && getInstructionAt(t) == null) {
                        disassemble(t);
                        if (createFunction(t, null) != null) created++;
                    } else if (tb != null && tb.isExecute() && getFunctionAt(t) == null
                        && getFunctionContaining(t) == null && getInstructionAt(t) != null) {
                        if (createFunction(t, null) != null) created++;
                    }
                } catch (Exception e) {}
                a = a.add(4);
            }
          }
        }
        println("created " + created);
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
