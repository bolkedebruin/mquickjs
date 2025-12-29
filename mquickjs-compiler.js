/**
 * MicroQuickJS Bytecode Compiler for Browser
 * Wrapper around mquickjs.wasm compiled with Emscripten
 */

class MQuickJSCompiler {
    constructor() {
        this.module = null;
        this.ready = false;
        this.initPromise = null;
    }

    /**
     * Initialize the WASM module
     */
    async init() {
        if (this.ready) return;

        // If already initializing, return the existing promise
        if (this.initPromise) {
            return this.initPromise;
        }

        this.initPromise = (async () => {
            try {
                console.log('Loading MQuickJS WASM module...');
                // Load the Emscripten-generated module
                this.module = await createMQuickJSModule();
                this.ready = true;
                console.log('MQuickJS compiler ready');
            } catch (err) {
                console.error('Failed to load MQuickJS module:', err);
                throw err;
            }
        })();

        return this.initPromise;
    }

    /**
     * Compile JavaScript source to bytecode
     *
     * @param {string} sourceCode - JavaScript source code
     * @param {object} options - Compilation options
     * @param {number} options.targetAddr - Flash address for pre-relocation (0 = none)
     * @param {boolean} options.use32Bit - Generate 32-bit bytecode (default: true for ESP32)
     * @returns {Promise<Uint8Array>} - Compiled bytecode
     * @throws {Error} - Compilation error with message
     */
    async compile(sourceCode, options = {}) {
        if (!this.ready) {
            await this.init();
        }

        const targetAddr = options.targetAddr || 0;
        const use32Bit = options.use32Bit !== false; // Default true

        // Allocate memory for source code
        const sourceLen = sourceCode.length;
        const sourcePtr = this.module._malloc(sourceLen + 1);

        try {
            // Copy source code to WASM memory
            this.module.stringToUTF8(sourceCode, sourcePtr, sourceLen + 1);

            // Call compilation function
            const result = this.module._compile_js_to_bytecode(
                sourcePtr,
                sourceLen,
                targetAddr,
                use32Bit ? 1 : 0
            );

            if (result < 0) {
                // Compilation failed - get error message
                const errorPtr = this.module._get_error_message();
                const errorMsg = this.module.UTF8ToString(errorPtr);
                throw new Error(errorMsg);
            }

            // Get bytecode buffer
            const bytecodePtr = this.module._get_bytecode_buffer();
            const bytecodeSize = this.module._get_bytecode_size();

            // Copy bytecode to JavaScript Uint8Array
            const bytecode = new Uint8Array(bytecodeSize);
            bytecode.set(this.module.HEAPU8.subarray(bytecodePtr, bytecodePtr + bytecodeSize));

            return bytecode;

        } finally {
            // Free allocated memory
            this.module._free(sourcePtr);
        }
    }

    /**
     * Validate JavaScript syntax without generating full bytecode
     *
     * @param {string} sourceCode - JavaScript source code
     * @returns {Promise<boolean>} - true if valid
     * @throws {Error} - Compilation error with message
     */
    async validate(sourceCode) {
        try {
            await this.compile(sourceCode);
            return true;
        } catch (err) {
            throw err;
        }
    }

    /**
     * Get version information
     */
    getVersion() {
        return {
            compiler: 'MicroQuickJS with FreeButton stdlib',
            features: ['LED', 'Button', 'Sensor', 'MQTT'],
            platform: 'WebAssembly'
        };
    }
}

// Export singleton instance
const mqjsCompiler = new MQuickJSCompiler();

// For module environments
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { MQuickJSCompiler, mqjsCompiler };
}
