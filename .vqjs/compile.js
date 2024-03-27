const compilerOptions = {
    target: ts.ModuleKind.ESNext,
    module: ts.ScriptTarget.ESNext,
    experimentalDecorators: true
};

class TsCompile {
    static TransformSource(source) {
        return globalThis.transformSource ? globalThis.transformSource(source) : source;
    }

    static compileFromFile(inFile, outFile, module = "root") {
        const source = fs.read(inFile);
        const result = TsCompile.compileFromSource(
            inFile.split(".").pop(),
            TsCompile.addMetadata(inFile) + TsCompile.TransformSource(source),
            module,
        );
        fs.write(outFile, result);
    }

    static compileFromSource(name, source, moduleName) {
        // transpileModule is kinda weird... because he cant resolve modules
        const output = ts.transpileModule(source, {
            compilerOptions,
            name,
            reportDiagnostics: true,
            moduleName,
        });
        return output.outputText;
    }

    static addMetadata(filename) {
        return globalThis.handleMetadata ? globalThis.handleMetadata(filename) : ``;
    }
}

globalThis.compile = TsCompile.compileFromFile;