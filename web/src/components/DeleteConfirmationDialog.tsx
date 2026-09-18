import { useEffect, useRef, type RefObject } from 'react'

interface DeleteConfirmationDialogProps {
  title: string
  itemType: string
  date?: string | null
  consequence: string
  cancelLabel: string
  confirmLabel: string
  busy: boolean
  error: string
  returnFocus: RefObject<HTMLElement | null>
  onCancel: () => void
  onConfirm: () => void
}

export function DeleteConfirmationDialog(props: DeleteConfirmationDialogProps) {
  const dialog = useRef<HTMLDivElement>(null)
  const cancel = useRef<HTMLButtonElement>(null)

  useEffect(() => {
    cancel.current?.focus()
    const returnTarget = props.returnFocus.current
    return () => returnTarget?.focus()
  }, [props.returnFocus])

  const handleKeyDown = (event: React.KeyboardEvent<HTMLDivElement>) => {
    if (event.key === 'Escape' && !props.busy) {
      event.preventDefault()
      props.onCancel()
      return
    }
    if (event.key !== 'Tab') return
    const buttons = dialog.current?.querySelectorAll<HTMLButtonElement>('button:not(:disabled)')
    if (!buttons || buttons.length === 0) return
    const first = buttons[0]
    const last = buttons[buttons.length - 1]
    if (event.shiftKey && document.activeElement === first) {
      event.preventDefault()
      last.focus()
    } else if (!event.shiftKey && document.activeElement === last) {
      event.preventDefault()
      first.focus()
    }
  }

  return <div className="delete-dialog-backdrop">
    <div
      ref={dialog}
      className="delete-confirmation"
      role="alertdialog"
      aria-modal="true"
      aria-labelledby="delete-dialog-title"
      aria-describedby="delete-dialog-consequence"
      onKeyDown={handleKeyDown}
    >
      <p className="eyebrow">{props.itemType}</p>
      <h2 id="delete-dialog-title">{props.confirmLabel} « {props.title} » ?</h2>
      {props.date && <p>{props.date}</p>}
      <p id="delete-dialog-consequence">{props.consequence}</p>
      {props.error && <p className="form-error" role="alert">{props.error}</p>}
      <div className="confirmation-actions">
        <button
          ref={cancel}
          type="button"
          className="quiet-action"
          disabled={props.busy}
          onClick={props.onCancel}
        >{props.cancelLabel}</button>
        <button
          type="button"
          className="danger-action"
          disabled={props.busy}
          onClick={props.onConfirm}
        >{props.busy ? 'Suppression…' : props.confirmLabel}</button>
      </div>
    </div>
  </div>
}
