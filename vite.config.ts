import { defineConfig } from "vite";
import { resolve } from "path";
import dts from "vite-plugin-dts";

export default defineConfig({
  build: {
    lib: {
      entry: {
        index: resolve(__dirname, "js/index.ts"),
        main: resolve(__dirname, "js/main.ts"),
        preload: resolve(__dirname, "js/preload.ts"),
      },
    },
    rollupOptions: {
      external: ["bindings", "electron", "node-addon-api"],
      output: [
        {
          format: "cjs",
          dir: "dist/cjs",
          entryFileNames: "[name].cjs",
          exports: "named",
        },
        {
          format: "es",
          dir: "dist/mjs",
          entryFileNames: "[name].mjs",
        },
      ],
    },
    sourcemap: false,
    minify: false,
    emptyOutDir: true,
  },
  plugins: [
    dts({
      outDir: "dist/types",
      copyDtsFiles: true,
      compilerOptions: {
        declarationMap: false,
      },
    }),
  ],
  esbuild: {
    target: "node16",
  },
});
