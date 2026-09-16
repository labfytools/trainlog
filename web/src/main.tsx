import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { App } from './app/App'
import 'react-grid-layout/css/styles.css'
import 'react-resizable/css/styles.css'
import './theme/tokens.css'
import './theme/app.css'

const root = document.getElementById('root')
if (root === null) throw new Error('Racine Trainlog absente')

createRoot(root).render(<StrictMode><App /></StrictMode>)
