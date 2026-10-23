import { motion, useReducedMotion } from 'motion/react'
import { useNavigate } from 'react-router-dom'
import { useHeart } from '../../context/useHeart'
import type { HeartLayout, ViewportSize } from './heartLayout'
import { HUB_FEATURES } from './hubFeatures'

interface HeartHubProps {
  layout: HeartLayout
  viewport: ViewportSize
}

/** Les fonctionnalités en orbite autour du cœur. Ce sont de vrais liens, accessibles au clavier. */
export function HeartHub({ layout, viewport }: HeartHubProps) {
  const navigate = useNavigate()
  const { react } = useHeart()
  const reduceMotion = useReducedMotion()

  // Ellipse qui reste dans l'écran, même sur mobile
  const radiusX = Math.min(layout.size * 1.3, viewport.width / 2 - 64)
  const radiusY = Math.min(layout.size * 0.95, layout.y - 56, viewport.height - layout.y - 56)
  const step = (Math.PI * 2) / HUB_FEATURES.length

  return (
    <nav className="heart-hub" aria-label="Fonctionnalités">
      <ul>
        {HUB_FEATURES.map((feature, index) => {
          const angle = -Math.PI / 2 + index * step
          const x = layout.x + Math.cos(angle) * radiusX
          const y = layout.y + Math.sin(angle) * radiusY

          return (
            <motion.li
              key={feature.path}
              className="hub-orbit"
              style={{ left: x, top: y }}
              initial={reduceMotion ? false : {
                opacity: 0,
                scale: 0.3,
                x: layout.x - x,
                y: layout.y - y,
              }}
              animate={{ opacity: 1, scale: 1, x: 0, y: 0 }}
              exit={reduceMotion ? undefined : {
                opacity: 0,
                scale: 0.3,
                x: layout.x - x,
                y: layout.y - y,
                transition: { duration: 0.28, delay: index * 0.02 },
              }}
              transition={{ type: 'spring', stiffness: 170, damping: 18, delay: 0.08 + index * 0.06 }}
            >
              <a
                className="hub-bubble"
                href={feature.path}
                style={{ animationDelay: `${index * -1.3}s` }}
                onClick={(event) => {
                  event.preventDefault()
                  react('nod')
                  navigate(feature.path)
                }}
              >
                <span className="hub-icon">{feature.icon}</span>
                <span className="hub-label">{feature.label}</span>
              </a>
            </motion.li>
          )
        })}
      </ul>
    </nav>
  )
}
