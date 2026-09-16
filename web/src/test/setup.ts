import '@testing-library/jest-dom/vitest'
import { afterEach, vi } from 'vitest'
import { cleanup } from '@testing-library/react'

Object.defineProperty(window, 'scrollTo', { value: vi.fn(), writable: true })
Object.defineProperty(window, 'innerWidth', { value: 1440, writable: true })
Object.defineProperty(window, 'matchMedia', { value: vi.fn().mockReturnValue({ matches: false, addEventListener: vi.fn(), removeEventListener: vi.fn() }), writable: true })
class TestResizeObserver {
  observe() {}
  unobserve() {}
  disconnect() {}
}
Object.defineProperty(window, 'ResizeObserver', { value: TestResizeObserver, writable: true })
Object.defineProperty(globalThis, 'ResizeObserver', { value: TestResizeObserver, writable: true })
Object.defineProperty(HTMLCanvasElement.prototype, 'getContext', {
  value: vi.fn(() => ({ measureText: (text: string) => ({ width: text.length * 7 }) })),
  writable: true,
})

afterEach(() => {
  cleanup()
  vi.restoreAllMocks()
  window.history.replaceState(null, '', '/')
})
