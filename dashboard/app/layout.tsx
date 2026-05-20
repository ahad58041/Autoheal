import "./globals.css";
import type { Metadata } from "next";

export const metadata: Metadata = {
  title: "AutoHeal Dashboard",
  description: "Self-healing process manager — live monitor",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="min-h-screen bg-bg text-slate-200">{children}</body>
    </html>
  );
}
