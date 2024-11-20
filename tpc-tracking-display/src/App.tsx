import './App.css'
import {Route, Routes} from "react-router-dom";
import TrackingDisplay from "./components/TrackingDisplay.tsx";
import RawDataDisplay from "./components/RawDataDisplay.tsx";

function App() {


  return (
      <Routes>
          <Route path="/" element={<TrackingDisplay />} />
          <Route path="/raw-data" element={<RawDataDisplay />} />
      </Routes>
  )
}

export default App
