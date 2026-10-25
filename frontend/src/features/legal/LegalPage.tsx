import { Navigate, useParams } from 'react-router-dom'
import { PagePanel } from '../../layout/PagePanel'
import { LEGAL_NAVIGATION, type LegalSlug } from '../../layout/navigation'

/** Plan prévu pour chaque page : le contenu reste à rédiger. */
const OUTLINES: Record<LegalSlug, string[]> = {
  conditions: ['Objet du service', 'Inscription et compte', 'Règles de conduite', 'Signalement et modération', 'Résiliation'],
  confidentialite: ['Données collectées', 'Finalités', 'Durée de conservation', 'Vos droits', 'Contact du responsable'],
  'mentions-legales': ['Éditeur', 'Hébergement', 'Propriété intellectuelle'],
  contact: ['Nous écrire', 'Signaler un problème'],
}

export function LegalPage() {
  const { page = '' } = useParams()
  const entry = LEGAL_NAVIGATION.find((item) => item.slug === page)
  if (!entry) return <Navigate to="/" replace />

  return (
    <PagePanel eyebrow="Informations" title={entry.label} description="Cette page sera bientôt rédigée.">
      <section className="section">
        <h2>Sommaire prévu</h2>
        <ol>
          {OUTLINES[entry.slug].map((heading) => <li key={heading}>{heading}</li>)}
        </ol>
      </section>
    </PagePanel>
  )
}
