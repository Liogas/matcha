import { Link } from 'react-router-dom'
import { PHASE_LABELS, SEASON_LABELS, useForest, useForestSummary } from '../forest-environment'
import { LEGAL_NAVIGATION } from './navigation'
import './site-footer.css'

export function SiteFooter() {
  const summary = useForestSummary()
  const { animated, setAnimated } = useForest()
  const sky = [SEASON_LABELS[summary.season], PHASE_LABELS[summary.phase]]
  if (summary.phase === 'night' || summary.phase === 'dusk') sky.push(summary.moonName)
  if (summary.snowing) sky.push('Il neige')

  return (
    <footer className="site-footer">
      <div className="site-footer__inner">
        <p className="site-footer__sky">{sky.join(' · ')}</p>

        <nav className="site-footer__nav" aria-label="Informations">
          <ul className="site-footer__links">
            {LEGAL_NAVIGATION.map((item) => (
              <li key={item.slug}>
                <Link to={`/infos/${item.slug}`}>{item.label}</Link>
              </li>
            ))}
          </ul>
        </nav>

        <div className="site-footer__meta">
          <button
            className="site-footer__toggle"
            type="button"
            aria-pressed={animated}
            onClick={() => setAnimated(!animated)}
          >
            {animated ? 'Figer le décor' : 'Animer le décor'}
          </button>
          <span>© {new Date().getFullYear()} Matcha</span>
        </div>
      </div>
    </footer>
  )
}
