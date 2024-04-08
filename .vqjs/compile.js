const compilerOptions = {
    target: ts.ModuleKind.ESNext,
    module: ts.ScriptTarget.ESNext,
    experimentalDecorators: true,
    useDefineForClassFields: false
};

class TsCompile {
    static TransformSource(source, file) {
        return globalThis.transformSource ? globalThis.transformSource(source, file) : source;
    }

    static compileFromFile(inFile, outFile, module = "root") {
        const source = fs.read(inFile);
        const result = TsCompile.compileFromSource(
            inFile.split(".").pop(),
            TsCompile.addMetadata(inFile) + TsCompile.TransformSource(source, inFile),
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

    static writeConfig(output, files = {}) {
        const options = {
            compilerOptions: {
                target: "ESNext",
                module: "ESNext",
                experimentalDecorators: true,
                useDefineForClassFields: false,
                paths: {}
            },
            includes: []
        };
        for (let file in files) {
            options.compilerOptions.paths[file] = [files[file]];
            options.includes.push(files[file] + "*/*")
        }
        fs.write(output, JSON.stringify(options, null, 2))
    }
}

globalThis.compile = TsCompile.compileFromFile;
globalThis.writeConfig = TsCompile.writeConfig;