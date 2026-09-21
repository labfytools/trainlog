import { Component, type ErrorInfo, type ReactNode } from "react";

interface SectionErrorBoundaryProps {
  children: ReactNode;
  fallbackTitle: string;
  retryLabel: string;
}

interface SectionErrorBoundaryState {
  failed: boolean;
}

export class SectionErrorBoundary extends Component<
  SectionErrorBoundaryProps,
  SectionErrorBoundaryState
> {
  state: SectionErrorBoundaryState = { failed: false };

  static getDerivedStateFromError(): SectionErrorBoundaryState {
    return { failed: true };
  }

  componentDidCatch(error: Error, information: ErrorInfo) {
    // CONTRACT: retain the surrounding Trainlog shell while preserving enough
    // browser diagnostics to investigate an unexpected section render failure.
    console.error("Trainlog section render failed", error, information);
  }

  render() {
    if (this.state.failed) {
      return (
        <article
          className="analysis-panel analysis-wide section-error"
          role="alert"
        >
          <h2>{this.props.fallbackTitle}</h2>
          <button
            type="button"
            className="quiet-action"
            onClick={() => this.setState({ failed: false })}
          >
            {this.props.retryLabel}
          </button>
        </article>
      );
    }
    return this.props.children;
  }
}
