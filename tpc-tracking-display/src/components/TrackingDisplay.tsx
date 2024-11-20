// src/components/TrackingDisplay.tsx
import React, { useEffect } from 'react';
import {
  Box,
  Typography,
  TextField,
  Button,
  Slider,
  Alert,
} from '@mui/material';
import { Link } from 'react-router-dom';
import Plot from 'react-plotly.js';
import { useStore } from '../store/useStore';
import { loadEventData } from '../utils/DataLoader';
import FileUploader from './FileUploader';
import {
  getPadCenter
} from '../utils/Geometry';

const TrackingDisplay: React.FC = () => {
  const events = useStore((state) => state.events);
  const currentEventNumber = useStore((state) => state.currentEventNumber);
  const currentEvent = useStore((state) => state.currentEvent);
  const time = useStore((state) => state.time);
  const maxTime = useStore((state) => state.maxTime);
  const setEvents = useStore((state) => state.setEvents);
  const setCurrentEventNumber = useStore((state) => state.setCurrentEventNumber);
  const setCurrentEvent = useStore((state) => state.setCurrentEvent);
  const setTime = useStore((state) => state.setTime);
  const setMaxTime = useStore((state) => state.setMaxTime);

  useEffect(() => {
    // Load default event data
    loadEventData('track_data.txt').then((loadedEvents) => {
      setEvents(loadedEvents);
      const eventNumbers = Object.keys(loadedEvents).map(Number);
      const firstEventNumber = eventNumbers[0];
      setCurrentEventNumber(firstEventNumber);
      setCurrentEvent(loadedEvents[firstEventNumber]);
    });
  }, [setEvents, setCurrentEventNumber, setCurrentEvent]);

  useEffect(() => {
    if (currentEvent) {
      const times = currentEvent.hits.map((hit) => hit[0]);
      const maxTimeValue = Math.max(...times);
      setMaxTime(maxTimeValue);
      setTime(maxTimeValue);
    }
  }, [currentEvent, setMaxTime, setTime]);

  const handleEventNumberChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setCurrentEventNumber(parseInt(e.target.value, 10));
  };

  const handleShowEvent = () => {
    if (
      currentEventNumber !== null &&
      events[currentEventNumber] !== undefined
    ) {
      setCurrentEvent(events[currentEventNumber]);
    } else {
      alert(`Event number ${currentEventNumber} not found.`);
    }
  };

  const handleNextEvent = () => {
    const eventNumbers = Object.keys(events)
      .map(Number)
      .sort((a, b) => a - b);
    if (currentEventNumber !== null) {
      const currentIndex = eventNumbers.indexOf(currentEventNumber);
      const nextIndex = (currentIndex + 1) % eventNumbers.length;
      const nextEventNumber = eventNumbers[nextIndex];
      setCurrentEventNumber(nextEventNumber);
      setCurrentEvent(events[nextEventNumber]);
    }
  };

  // Prepare data for plots
  /* eslint-disable @typescript-eslint/no-explicit-any */
  let plotDataXY: any[] = [];
  let plotDataZY: any[] = [];
  let planeList: string[] = [];
  /* eslint-enable @typescript-eslint/no-explicit-any */

  if (currentEvent) {
    const filteredHits = currentEvent.hits.filter((hit) => hit[0] <= time);
    const xs: number[] = [];
    const ys: number[] = [];
    const zs: number[] = [];
    const planes: string[] = [];


    filteredHits.forEach((hit) => {

      // eslint-disable-next-line @typescript-eslint/no-unused-vars
      const [timeArrival, amplitude, ring, pad, plane, zToGEM] = hit;
      const [x, y] = getPadCenter(ring, pad);
      const z = plane * planeSpacingCm + zToGEM * 100;
      xs.push(x);
      ys.push(y);
      zs.push(z);
      planes.push(`Plane ${plane}`);
    });

    planeList = [...new Set(planes)];

    // Define color mapping for planes
    const planeColors: { [key: string]: string } = {};
    const colorsPalette = [
      'red',
      'orange',
      'yellow',
      'green',
      'blue',
      'indigo',
      'violet',
      'pink',
      'cyan',
      'magenta',
    ];
    planeList.forEach((plane, index) => {
      planeColors[plane] = colorsPalette[index % colorsPalette.length];
    });

    const colors = planes.map((plane) => planeColors[plane]);

    plotDataXY = [
      {
        x: xs,
        y: ys,
        mode: 'markers',
        type: 'scatter',
        marker: { color: colors, size: 6 },
        text: planes,
        hovertemplate: 'Plane: %{text}<br>X: %{x}<br>Y: %{y}<extra></extra>',
      },
    ];

    plotDataZY = [
      {
        x: zs,
        y: ys,
        mode: 'markers',
        type: 'scatter',
        marker: { color: colors, size: 6 },
        text: planes,
        hovertemplate: 'Plane: %{text}<br>Z: %{x}<br>Y: %{y}<extra></extra>',
      },
    ];
  }

  return (
    <Box p={2}>
      <Typography variant="h4" gutterBottom>
        Tracking Display
      </Typography>
      <Box display="flex" alignItems="center" mb={2}>
        <TextField
          label="Event Number"
          value={currentEventNumber !== null ? currentEventNumber : ''}
          onChange={handleEventNumberChange}
          variant="outlined"
          size="small"
          style={{ marginRight: '8px' }}
        />
        <Button
          variant="contained"
          color="primary"
          onClick={handleShowEvent}
          style={{ marginRight: '8px' }}
        >
          Show
        </Button>
        <Button
          variant="contained"
          color="secondary"
          onClick={handleNextEvent}
          style={{ marginRight: '8px' }}
        >
          Next Track
        </Button>
        <FileUploader />
        <Button
          variant="contained"
          component={Link}
          to="/raw-data"
          style={{ marginLeft: 'auto' }}
        >
          View Raw Data
        </Button>
      </Box>
      {currentEvent && (
        <Box mb={2}>
          <Typography>Time (ns): {time.toFixed(2)}</Typography>
          <Slider
            value={time}
            min={0}
            max={maxTime}
            step={0.1}
            onChange={(_, newValue) => setTime(newValue as number)}
          />
        </Box>
      )}
      {currentEvent ? (
        <Box display="flex">
          <Plot
            data={plotDataZY}
            layout={{
              title: 'Z vs Y',
              xaxis: { title: 'Z (cm)' },
              yaxis: { title: 'Y (cm)' },
              width: 600,
              height: 600,
              showlegend: false,
            }}
          />
          <Plot
            data={plotDataXY}
            layout={{
              title: 'X vs Y',
              xaxis: { title: 'X (cm)' },
              yaxis: { title: 'Y (cm)' },
              width: 600,
              height: 600,
              showlegend: false,
            }}
          />
        </Box>
      ) : (
        <Alert severity="info">No event data to display.</Alert>
      )}
    </Box>
  );
};

export default TrackingDisplay;
