import { useEffect, useRef, useState } from 'react'
import { getImageUrl } from '../../api/auth'
import { useAuth } from '../../context/useAuth'

/**
 * Les photos sont servies derrière le JWT : on les télécharge avec l'en-tête
 * Authorization puis on les affiche via une URL blob, libérée au démontage.
 */
function useAuthorizedImage(path: string | undefined) {
  const { accessToken } = useAuth()
  // Le jeton est renouvelé régulièrement : inutile de recharger l'image pour autant
  const tokenRef = useRef(accessToken)
  const [loaded, setLoaded] = useState<{ path: string; url: string } | null>(null)

  useEffect(() => {
    tokenRef.current = accessToken
  }, [accessToken])

  useEffect(() => {
    const token = tokenRef.current
    if (!path || !token) return
    let active = true
    let url = ''
    void getImageUrl(token, path)
      .then((objectUrl) => {
        url = objectUrl
        if (active) setLoaded({ path, url: objectUrl })
        else URL.revokeObjectURL(objectUrl)
      })
      .catch(() => undefined)
    return () => {
      active = false
      if (url) URL.revokeObjectURL(url)
    }
  }, [path])

  return loaded && loaded.path === path ? loaded.url : ''
}

/** Portrait d'un profil : la photo, ou l'initiale du prénom en attendant. */
export function ProfileImage({
  path,
  name,
  className = '',
}: {
  path?: string
  name: string
  className?: string
}) {
  const url = useAuthorizedImage(path)
  return (
    <div className={`portrait ${className}`}>
      {url
        ? <img src={url} alt={`Portrait de ${name}`} />
        : <span aria-hidden="true">{name.charAt(0).toUpperCase()}</span>}
    </div>
  )
}
