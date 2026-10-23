import { useEffect, useState } from 'react'

/**
 * hub   : le cœur au centre, les fonctionnalités en orbite autour de lui.
 * side  : une fonctionnalité est ouverte, le cœur se range à gauche (en haut sur mobile).
 * stage : connexion / inscription, grand cœur au-dessus du formulaire.
 */
export type HeartLayoutMode = 'hub' | 'side' | 'stage'

/** Centre et hauteur du cœur, en pixels CSS. Partagé par la scène 3D et le HTML. */
export interface HeartLayout {
  x: number
  y: number
  size: number
}

export interface ViewportSize {
  width: number
  height: number
}

/** En dessous, le mode « side » place le cœur au-dessus du panneau. */
export const SIDE_BREAKPOINT = 860
/** Hauteur réservée au cœur en haut de l'écran sur mobile (mode side). */
export const MOBILE_HEART_BAND = 116

const clamp = (value: number, min: number, max: number) =>
  Math.min(max, Math.max(min, value))

/** Doit rester identique à `--side-column` dans App.css. */
export function sideColumnWidth(width: number) {
  return clamp(width * 0.34, 280, 460)
}

export function computeHeartLayout(mode: HeartLayoutMode, { width, height }: ViewportSize): HeartLayout {
  switch (mode) {
    case 'hub': {
      const size = Math.min(height * 0.32, width * 0.38)
      return { x: width / 2, y: height * 0.52, size }
    }
    case 'side': {
      if (width < SIDE_BREAKPOINT)
        return { x: width / 2, y: MOBILE_HEART_BAND / 2 + 6, size: 78 }
      const column = sideColumnWidth(width)
      return { x: column / 2, y: height * 0.5, size: Math.min(column * 0.62, height * 0.38) }
    }
    case 'stage': {
      const size = clamp(Math.min(height * 0.24, width * 0.42), 96, 230)
      return { x: width / 2, y: 64 + size / 2, size }
    }
  }
}

/** Convertit un point en pixels vers les coordonnées normalisées de three.js (-1..1). */
export function toNormalized(x: number, y: number, viewport: ViewportSize) {
  return {
    x: (x / viewport.width) * 2 - 1,
    y: -(y / viewport.height) * 2 + 1,
  }
}

function readViewport(): ViewportSize {
  return { width: window.innerWidth, height: window.innerHeight }
}

export function useViewportSize() {
  const [viewport, setViewport] = useState(readViewport)

  useEffect(() => {
    const update = () => setViewport(readViewport())
    window.addEventListener('resize', update)
    return () => window.removeEventListener('resize', update)
  }, [])

  return viewport
}
