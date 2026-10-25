import { NavLink } from 'react-router-dom'
import { useSocialBadges } from '../context/useSocial'
import { MAIN_NAVIGATION } from './navigation'

/** Navigation principale ; Matchs et Messages portent un compteur discret. */
export function MainNav() {
  const { unreadMessages, pendingLikes } = useSocialBadges()
  const badges: Partial<Record<string, { count: number; label: string }>> = {
    '/matchs': { count: pendingLikes, label: `${pendingLikes} like${pendingLikes > 1 ? 's' : ''} en attente` },
    '/messages': { count: unreadMessages, label: `${unreadMessages} message${unreadMessages > 1 ? 's' : ''} non lu${unreadMessages > 1 ? 's' : ''}` },
  }

  return (
    <nav className="main-nav" aria-label="Navigation principale">
      <ul className="main-nav__list">
        {MAIN_NAVIGATION.map((item) => {
          const badge = badges[item.path]
          return (
            <li key={item.path}>
              <NavLink className="main-nav__link" to={item.path}>
                {item.label}
                {badge && badge.count > 0 && (
                  <span className="main-nav__badge">
                    <span aria-hidden="true">{badge.count > 99 ? '99+' : badge.count}</span>
                    <span className="visually-hidden">, {badge.label}</span>
                  </span>
                )}
              </NavLink>
            </li>
          )
        })}
      </ul>
    </nav>
  )
}
