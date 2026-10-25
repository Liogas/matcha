// Les styles globaux d'abord : ils déclarent l'ordre des couches CSS
// avant que les composants n'importent leurs propres fichiers.
import './styles/index.css'
import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { App } from './app/App'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
