import { defineConfig } from "vite";
import { resolve } from "path";
import dts from "vite-plugin-dts";
import { readFileSync } from "fs";
import { builtinModules } from "module";

// 读取package.json获取依赖列表
const pkg = JSON.parse(
  readFileSync(resolve(__dirname, "package.json"), "utf-8")
);

// 自动生成external列表
const external = [
  // Node.js内置模块
  ...builtinModules,
  // Node.js内置模块的node:前缀版本
  ...builtinModules.map((m) => `node:${m}`),
  // 生产依赖
  ...Object.keys(pkg.dependencies || {}),
  // 开发依赖（如electron等）
  ...Object.keys(pkg.devDependencies || {}),
  // peerDependencies（如果有的话）
  ...Object.keys(pkg.peerDependencies || {}),
];

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
      external,
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
