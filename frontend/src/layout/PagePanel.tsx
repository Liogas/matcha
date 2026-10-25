import { motion, useReducedMotion } from 'motion/react'
import { useLayoutEffect, useRef } from 'react'
import type { ReactNode } from 'react'
import { useForest } from '../forest-environment'
import './page-panel.css'

/** À partir de cette largeur, le panneau est à droite et le décor se recadre à gauche. */
const SIDE_LAYOUT = '(min-width: 64rem)'

/**
 * Le support de chaque page : un panneau de papier (de jour) ou de sous-bois (la nuit),
 * posé à droite du décor. Il signale au décor la place qu'il occupe.
 */
export function PagePanel({
  eyebrow,
  title,
  description,
  actions,
  footer,
  children,
  size = 'regular',
}: {
  eyebrow?: string
  title: string
  description?: ReactNode
  actions?: ReactNode
  footer?: ReactNode
  children: ReactNode
  size?: 'narrow' | 'regular'
}) {
  const reduceMotion = useReducedMotion()
  const { setRightInset } = useForest()
  const panelRef = useRef<HTMLElement>(null)

  useLayoutEffect(() => {
    const panel = panelRef.current
    if (!panel) return
    const media = window.matchMedia(SIDE_LAYOUT)
    const report = () => {
      const left = panel.getBoundingClientRect().left
      setRightInset(media.matches ? Math.max(0, (window.innerWidth - left) / window.innerWidth) : 0)
    }
    report()
    const observer = new ResizeObserver(report)
    observer.observe(panel)
    media.addEventListener('change', report)
    return () => {
      observer.disconnect()
      media.removeEventListener('change', report)
    }
  }, [setRightInset])

  return (
    <motion.section
      ref={panelRef}
      className={`page-panel page-panel--${size}`}
      aria-labelledby="page-title"
      initial={reduceMotion ? false : { opacity: 0, y: 14 }}
      animate={{ opacity: 1, y: 0 }}
      exit={reduceMotion ? undefined : { opacity: 0, y: -8 }}
      transition={{ duration: reduceMotion ? 0 : 0.35, ease: [0.22, 1, 0.36, 1] }}
    >
      <header className="page-panel__header">
        <div className="page-panel__heading">
          {eyebrow && <p className="page-panel__eyebrow">{eyebrow}</p>}
          <h1 id="page-title">{title}</h1>
          {description && <p className="page-panel__description">{description}</p>}
        </div>
        {actions && <div className="page-panel__actions">{actions}</div>}
      </header>
      <div className="page-panel__body">{children}</div>
      {footer && <footer className="page-panel__footer">{footer}</footer>}
    </motion.section>
  )
}
