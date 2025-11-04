import React from 'react'

export const App: React.FC = () => {
  return (
    <div className="layout">
      <div className="panel left">
        <div className="title">Hierarchy</div>
        <div className="content">/ (empty)</div>
      </div>
      <div className="panel">
        <div className="title">Viewport</div>
        <div id="viewport"></div>
      </div>
      <div className="panel right">
        <div className="title">Inspector</div>
        <div className="content">Select an object</div>
      </div>
    </div>
  )
}
