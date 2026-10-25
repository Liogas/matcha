import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  build: {
    rolldownOptions: {
      output: {
        codeSplitting: {
          groups: [
            {
              name: 'three-vendor',
              test: /node_modules[\\/]three[\\/]/,
              maxSize: 400_000,
              priority: 20,
            },
            {
              name: 'react-vendor',
              test: /node_modules[\\/](react|react-dom|react-router|react-router-dom|motion|framer-motion|motion-dom|motion-utils|scheduler)[\\/]/,
              priority: 10,
            },
          ],
        },
      },
    },
  },
})
