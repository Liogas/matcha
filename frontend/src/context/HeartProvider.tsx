import { useEffect, useMemo, useState } from 'react'
import type { ReactNode } from 'react'
import { useReducedMotion } from 'motion/react'
import { LivingEntityController } from '../components/living-entity/LivingEntityController'
import { LivingEntityModel } from '../components/living-entity/LivingEntityModel'
import { HeartContext, type HeartContextValue } from './HeartContext'

const TEXT_INPUT_TYPES = new Set([
  'text', 'email', 'search', 'tel', 'url', 'number', 'date', 'password',
])

function isTextField(target: EventTarget | null): target is HTMLInputElement | HTMLTextAreaElement {
  if (target instanceof HTMLTextAreaElement)
    return true
  return target instanceof HTMLInputElement && TEXT_INPUT_TYPES.has(target.type)
}

/** Centre d'un élément en coordonnées normalisées (-1..1). */
function elementCenter(element: Element) {
  const rect = element.getBoundingClientRect()
  return {
    x: ((rect.left + rect.width / 2) / window.innerWidth) * 2 - 1,
    y: -((rect.top + rect.height / 2) / window.innerHeight) * 2 + 1,
  }
}

/**
 * Crée le cœur (modèle + contrôleur) une seule fois pour toute l'application
 * et lui transmet ce que fait l'utilisateur : pointeur, champ actif, frappe.
 * Les pages n'ont rien à faire pour que le cœur suive le formulaire ;
 * elles appellent `react()` pour les événements métier (like, match…).
 */
export function HeartProvider({ children }: { children: ReactNode }) {
  const [heart] = useState(() => {
    const model = new LivingEntityModel()
    return { model, controller: new LivingEntityController(model) }
  })
  const reduceMotion = useReducedMotion()

  useEffect(() => {
    heart.controller.setReducedMotion(reduceMotion ?? false)
  }, [heart, reduceMotion])

  useEffect(() => {
    const { controller } = heart

    const onPointerMove = (event: PointerEvent) => {
      controller.setPointer({
        x: (event.clientX / window.innerWidth) * 2 - 1,
        y: -(event.clientY / window.innerHeight) * 2 + 1,
      })
    }
    const onActivity = () => controller.notifyActivity()

    const onFocusIn = (event: FocusEvent) => {
      if (!isTextField(event.target))
        return
      const hidden = event.target instanceof HTMLInputElement && event.target.type === 'password'
      controller.setEyesHidden(hidden)
      controller.setFocus(hidden ? null : elementCenter(event.target))
    }
    const onFocusOut = (event: FocusEvent) => {
      if (!isTextField(event.target))
        return
      controller.setEyesHidden(false)
      controller.setFocus(null)
    }
    const onInput = (event: Event) => {
      if (!isTextField(event.target))
        return
      controller.react('tick')
      if (document.activeElement === event.target && event.target.type !== 'password')
        controller.setFocus(elementCenter(event.target))
    }

    window.addEventListener('pointermove', onPointerMove, { passive: true })
    window.addEventListener('pointerdown', onActivity, { passive: true })
    window.addEventListener('keydown', onActivity)
    window.addEventListener('wheel', onActivity, { passive: true })
    document.addEventListener('focusin', onFocusIn)
    document.addEventListener('focusout', onFocusOut)
    document.addEventListener('input', onInput)

    return () => {
      window.removeEventListener('pointermove', onPointerMove)
      window.removeEventListener('pointerdown', onActivity)
      window.removeEventListener('keydown', onActivity)
      window.removeEventListener('wheel', onActivity)
      document.removeEventListener('focusin', onFocusIn)
      document.removeEventListener('focusout', onFocusOut)
      document.removeEventListener('input', onInput)
    }
  }, [heart])

  const value = useMemo<HeartContextValue>(() => ({
    ...heart,
    react: (reaction) => heart.controller.react(reaction),
  }), [heart])

  return <HeartContext.Provider value={value}>{children}</HeartContext.Provider>
}
