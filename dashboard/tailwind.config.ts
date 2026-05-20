import type { Config } from "tailwindcss";

const config: Config = {
  content: [
    "./app/**/*.{ts,tsx}",
    "./components/**/*.{ts,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        // dark dashboard palette
        bg:        "#0b0f17",
        panel:     "#121826",
        panelAlt:  "#1a2233",
        accent:    "#38bdf8",
        danger:    "#ef4444",
        warning:   "#f59e0b",
        ok:        "#22c55e",
        muted:     "#64748b",
      },
    },
  },
  plugins: [],
};
export default config;
