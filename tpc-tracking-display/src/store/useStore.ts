// src/store/useStore.ts
import { create } from 'zustand';
import { combine } from 'zustand/middleware';
import { EventData } from '../utils/DataLoader';

interface StoreState {
    events: { [key: number]: EventData };
    currentEventNumber: number | null;
    currentEvent: EventData | null;
    time: number;
    maxTime: number;
}

interface StoreActions {
    setEvents: (events: { [key: number]: EventData }) => void;
    setCurrentEventNumber: (eventNumber: number) => void;
    setCurrentEvent: (event: EventData) => void;
    setTime: (time: number) => void;
    setMaxTime: (maxTime: number) => void;
}

export const useStore = create<StoreState & StoreActions>(
    combine(
        {
            events: {} as { [key: number]: EventData },
            currentEventNumber: null as number | null,
            currentEvent: null as EventData | null,
            time: 0,
            maxTime: 1,
        },
        (set) => ({
            setEvents: (events) => set({ events }),
            setCurrentEventNumber: (eventNumber) => set({ currentEventNumber: eventNumber }),
            setCurrentEvent: (event) => set({ currentEvent: event }),
            setTime: (time) => set({ time }),
            setMaxTime: (maxTime) => set({ maxTime }),
        })
    )
);
