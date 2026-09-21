import { fireEvent, render, screen } from "@testing-library/react";
import { expect, it, vi } from "vitest";
import { SectionErrorBoundary } from "./SectionErrorBoundary";

function BrokenSection(): never {
  throw new Error("render failure");
}

it("keeps the surrounding shell mounted and offers a local retry", () => {
  const consoleError = vi
    .spyOn(console, "error")
    .mockImplementation(() => undefined);
  render(
    <div data-testid="shell">
      <header>Trainlog</header>
      <SectionErrorBoundary
        fallbackTitle="Impossible d’afficher cette section."
        retryLabel="Réessayer"
      >
        <BrokenSection />
      </SectionErrorBoundary>
      <nav>Navigation</nav>
    </div>,
  );
  expect(screen.getByTestId("shell")).toBeInTheDocument();
  expect(screen.getByText("Trainlog")).toBeInTheDocument();
  expect(screen.getByText("Navigation")).toBeInTheDocument();
  expect(screen.getByRole("alert")).toHaveTextContent(
    "Impossible d’afficher cette section.",
  );
  fireEvent.click(screen.getByRole("button", { name: "Réessayer" }));
  expect(screen.getByTestId("shell")).toBeInTheDocument();
  expect(consoleError).toHaveBeenCalled();
  consoleError.mockRestore();
});
