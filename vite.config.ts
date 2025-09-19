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
    outDir: "dist",
    rollupOptions: {
      external: ["bindings", "electron", "node-addon-api", "events"],
      output: [
        {
          format: "cjs",
          entryFileNames: "[name].js",
          exports: "named",
        },
        {
          format: "es",
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
