import { motion, useReducedMotion } from 'motion/react'
import { Link, Navigate, useParams } from 'react-router-dom'
import { HUB_FEATURES } from '../components/heart-stage/hubFeatures'
import type { EntityReaction } from '../components/living-entity/LivingEntityModel'
import { useHeart } from '../context/useHeart'

/** Le hub n'a pas de panneau : seulement une légende sous l'orbite. */
export function HubPage() {
  const reduceMotion = useReducedMotion()
  return (
    <motion.p
      className="hub-caption"
      initial={reduceMotion ? false : { opacity: 0, y: 10 }}
      animate={{ opacity: 1, y: 0 }}
      exit={reduceMotion ? undefined : { opacity: 0 }}
      transition={{ delay: 0.4, duration: 0.5 }}
    >
      Choisissez une bulle, ou dites bonjour au cœur.
    </motion.p>
  )
}

/** Démonstrations provisoires, en attendant que les panneaux soient branchés sur l'API. */
const DEMOS: Record<string, { label: string; reaction: EntityReaction; upcoming: string }> = {
  decouvrir: {
    label: 'Simuler un like',
    reaction: 'love',
    upcoming: 'Les profils suggérés, avec la recherche et les filtres (âge, distance, intérêts, popularité).',
  },
  matchs: {
    label: 'Simuler un match',
    reaction: 'joy',
    upcoming: 'La liste de vos matchs, et l’écran de célébration quand un nouveau match arrive.',
  },
  messages: {
    label: 'Simuler un message reçu',
    reaction: 'startle',
    upcoming: 'Vos conversations en temps réel, branchées sur le WebSocket de messagerie.',
  },
  notifications: {
    label: 'Simuler une erreur',
    reaction: 'sad',
    upcoming: 'Likes reçus, visites de profil, nouveaux matchs et messages.',
  },
  profil: {
    label: 'Faire un câlin au cœur',
    reaction: 'poke',
    upcoming: 'Votre profil, vos photos, vos intérêts… et l’apparence de votre cœur.',
  },
}

export function FeaturePanel() {
  const { feature = '' } = useParams()
  const { react } = useHeart()
  const reduceMotion = useReducedMotion()
  const info = HUB_FEATURES.find((item) => item.path === `/app/${feature}`)
  const demo = DEMOS[feature]

  if (!info || !demo)
    return <Navigate to="/app" replace />

  return (
    <motion.section
      className="side-panel"
      initial={reduceMotion ? false : { opacity: 0, x: 40, filter: 'blur(8px)' }}
      animate={{ opacity: 1, x: 0, filter: 'blur(0px)' }}
      exit={reduceMotion ? undefined : { opacity: 0, x: 30, filter: 'blur(6px)' }}
      transition={{ duration: reduceMotion ? 0 : 0.4, ease: [0.22, 1, 0.36, 1] }}
      aria-labelledby="panel-title"
    >
      <Link className="back-link" to="/app">
        <span aria-hidden="true">←</span> Revenir au cœur
      </Link>
      <div className="panel-heading">
        <span className="panel-icon">{info.icon}</span>
        <div>
          <h1 id="panel-title">{info.label}</h1>
          <p className="panel-description">{info.hint}</p>
        </div>
      </div>

      <div className="placeholder-card">
        <p className="eyebrow">Bientôt ici</p>
        <p>{demo.upcoming}</p>
      </div>

      <div className="demo-box">
        <p>En attendant, regardez le cœur :</p>
        <button className="button button-ghost" type="button" onClick={() => react(demo.reaction)}>
          {demo.label}
        </button>
      </div>
    </motion.section>
  )
}
