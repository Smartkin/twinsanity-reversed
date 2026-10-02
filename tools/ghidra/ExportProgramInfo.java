// Dumps a program's functions, symbols, comments, data types, strings and a decompilation of every function.
// Arg 0: output directory. Arg 1 (optional): number of decompiler threads.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

public class ExportProgramInfo extends GhidraScript {

    private static String esc(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        out.mkdirs();
        int threads = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        Program p = currentProgram;
        Listing listing = p.getListing();
        println("Exporting " + p.getName() + " to " + out);

        try (PrintWriter w = new PrintWriter(new File(out, "memmap.txt"), StandardCharsets.UTF_8)) {
            for (MemoryBlock b : p.getMemory().getBlocks()) {
                w.println(b.getName() + "\t" + b.getStart() + "\t" + b.getEnd() + "\t" + b.getSize() + "\tinit=" + b.isInitialized() + "\tr=" + b.isRead() + " w=" + b.isWrite() + " x=" + b.isExecute());
            }
        }

        List<Function> functions = new ArrayList<>();
        try (PrintWriter w = new PrintWriter(new File(out, "functions.tsv"), StandardCharsets.UTF_8)) {
            w.println("address\tname\tsource\tsize\tnamespace\tsignature\tcalling\tthunk");
            FunctionIterator it = listing.getFunctions(true);
            while (it.hasNext()) {
                Function f = it.next();
                functions.add(f);
                long size = f.getBody().getNumAddresses();
                w.println(f.getEntryPoint() + "\t" + esc(f.getName()) + "\t" + f.getSymbol().getSource() + "\t" + size + "\t" + esc(f.getParentNamespace().getName(true)) + "\t" + esc(f.getPrototypeString(false, false)) + "\t" + f.getCallingConventionName() + "\t" + f.isThunk());
            }
        }
        println("Functions: " + functions.size());

        try (PrintWriter w = new PrintWriter(new File(out, "symbols.tsv"), StandardCharsets.UTF_8)) {
            w.println("address\tname\ttype\tsource\tnamespace\tprimary");
            SymbolIterator it = p.getSymbolTable().getAllSymbols(false);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (s.isDynamic()) continue;
                w.println(s.getAddress() + "\t" + esc(s.getName()) + "\t" + s.getSymbolType() + "\t" + s.getSource() + "\t" + esc(s.getParentNamespace().getName(true)) + "\t" + s.isPrimary());
            }
        }

        try (PrintWriter w = new PrintWriter(new File(out, "comments.tsv"), StandardCharsets.UTF_8)) {
            w.println("address\tkind\tcomment");
            AddressIterator it = listing.getCommentAddressIterator(p.getMemory(), true);
            String[] kinds = {"EOL", "PRE", "POST", "PLATE", "REPEATABLE"};
            int[] types = {CodeUnit.EOL_COMMENT, CodeUnit.PRE_COMMENT, CodeUnit.POST_COMMENT, CodeUnit.PLATE_COMMENT, CodeUnit.REPEATABLE_COMMENT};
            while (it.hasNext()) {
                Address a = it.next();
                for (int i = 0; i < types.length; i++) {
                    String c = listing.getComment(types[i], a);
                    if (c != null) w.println(a + "\t" + kinds[i] + "\t" + esc(c));
                }
            }
        }

        try (PrintWriter w = new PrintWriter(new File(out, "datatypes.txt"), StandardCharsets.UTF_8)) {
            DataTypeManager dtm = p.getDataTypeManager();
            Iterator<DataType> it = dtm.getAllDataTypes();
            while (it.hasNext()) {
                DataType dt = it.next();
                if (dt instanceof Structure || dt instanceof Union) {
                    Composite c = (Composite) dt;
                    w.println((dt instanceof Union ? "union " : "struct ") + dt.getPathName() + " size=0x" + Integer.toHexString(c.getLength()) + " {");
                    for (DataTypeComponent comp : c.getDefinedComponents()) {
                        w.println("    0x" + Integer.toHexString(comp.getOffset()) + "\t" + comp.getDataType().getName() + "\t" + comp.getFieldName() + (comp.getComment() != null ? "\t// " + esc(comp.getComment()) : ""));
                    }
                    w.println("}");
                } else if (dt instanceof ghidra.program.model.data.Enum) {
                    ghidra.program.model.data.Enum e = (ghidra.program.model.data.Enum) dt;
                    w.println("enum " + dt.getPathName() + " size=" + e.getLength() + " {");
                    for (String n : e.getNames()) {
                        w.println("    " + n + " = 0x" + Long.toHexString(e.getValue(n)) + (e.getComment(n) != null && !e.getComment(n).isEmpty() ? "\t// " + esc(e.getComment(n)) : ""));
                    }
                    w.println("}");
                } else if (dt instanceof TypeDef) {
                    w.println("typedef " + dt.getPathName() + " = " + ((TypeDef) dt).getDataType().getPathName());
                } else if (dt instanceof FunctionDefinition) {
                    w.println("funcdef " + dt.getPathName() + " : " + ((FunctionDefinition) dt).getPrototypeString());
                }
            }
        }

        try (PrintWriter w = new PrintWriter(new File(out, "data.tsv"), StandardCharsets.UTF_8)) {
            w.println("address\tlabel\ttype\tlength\tvalue");
            DataIterator it = listing.getDefinedData(true);
            while (it.hasNext()) {
                Data d = it.next();
                String label = d.getLabel();
                String rep;
                try { rep = d.getDefaultValueRepresentation(); } catch (Exception e) { rep = "?"; }
                if (rep != null && rep.length() > 300) rep = rep.substring(0, 300) + "...";
                w.println(d.getAddress() + "\t" + esc(label) + "\t" + esc(d.getDataType().getName()) + "\t" + d.getLength() + "\t" + esc(rep));
            }
        }

        // Raw scan for printable runs, whether or not they're defined as strings
        try (PrintWriter w = new PrintWriter(new File(out, "rawstrings.tsv"), StandardCharsets.UTF_8)) {
            w.println("address\tstring");
            for (MemoryBlock b : p.getMemory().getBlocks()) {
                if (!b.isInitialized()) continue;
                byte[] bytes = new byte[(int) b.getSize()];
                try { b.getBytes(b.getStart(), bytes); } catch (Exception e) { continue; }
                int start = -1;
                for (int i = 0; i <= bytes.length; i++) {
                    int c = i < bytes.length ? (bytes[i] & 0xFF) : 0;
                    boolean printable = (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\n' || c == '\r';
                    if (printable) {
                        if (start < 0) start = i;
                    } else {
                        if (start >= 0 && i - start >= 4 && c == 0) {
                            w.println(b.getStart().add(start) + "\t" + esc(new String(bytes, start, i - start, StandardCharsets.ISO_8859_1)));
                        }
                        start = -1;
                    }
                }
            }
        }

        // Decompile everything with a pool of decompilers
        File decompDir = new File(out, "decomp");
        decompDir.mkdirs();
        ExecutorService pool = Executors.newFixedThreadPool(threads);
        AtomicInteger done = new AtomicInteger();
        AtomicInteger failed = new AtomicInteger();
        List<List<Function>> chunks = new ArrayList<>();
        for (int i = 0; i < threads; i++) chunks.add(new ArrayList<>());
        for (int i = 0; i < functions.size(); i++) chunks.get(i % threads).add(functions.get(i));
        List<Future<?>> futures = new ArrayList<>();
        for (List<Function> chunk : chunks) {
            futures.add(pool.submit(() -> {
                DecompInterface ifc = new DecompInterface();
                DecompileOptions opts = new DecompileOptions();
                ifc.setOptions(opts);
                ifc.toggleCCode(true);
                ifc.toggleSyntaxTree(false);
                ifc.setSimplificationStyle("decompile");
                if (!ifc.openProgram(p)) {
                    printerr("Decompiler failed to open program: " + ifc.getLastMessage());
                    return;
                }
                try {
                    for (Function f : chunk) {
                        if (monitor.isCancelled()) return;
                        String fname = f.getEntryPoint().toString() + "_" + f.getName().replaceAll("[^A-Za-z0-9_.$~]", "_");
                        if (fname.length() > 120) fname = fname.substring(0, 120);
                        File file = new File(decompDir, fname + ".c");
                        try {
                            DecompileResults res = ifc.decompileFunction(f, 90, monitor);
                            String text;
                            if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                                text = res.getDecompiledFunction().getC();
                            } else {
                                text = "// DECOMPILE FAILED: " + (res != null ? res.getErrorMessage() : "null") + "\n";
                                failed.incrementAndGet();
                            }
                            try (PrintWriter w = new PrintWriter(file, StandardCharsets.UTF_8)) {
                                w.print(text);
                            }
                        } catch (Exception e) {
                            failed.incrementAndGet();
                            try (PrintWriter w = new PrintWriter(file, StandardCharsets.UTF_8)) {
                                w.println("// EXCEPTION: " + e);
                            } catch (Exception e2) { }
                        }
                        int n = done.incrementAndGet();
                        if (n % 500 == 0) println("Decompiled " + n + "/" + functions.size());
                    }
                } finally {
                    ifc.dispose();
                }
            }));
        }
        for (Future<?> fu : futures) fu.get();
        pool.shutdown();
        println("Decompiled " + done.get() + " functions, " + failed.get() + " failed");
    }
}
