// src/components/FileUploader.tsx
import React, { ChangeEvent } from 'react';
import { Button } from '@mui/material';
import UploadFileIcon from '@mui/icons-material/UploadFile';
import { useStore } from '../store/useStore';
import { loadEventData } from '../utils/DataLoader';

const FileUploader: React.FC = () => {
    const setEvents = useStore((state) => state.setEvents);
    const setCurrentEventNumber = useStore((state) => state.setCurrentEventNumber);
    const setCurrentEvent = useStore((state) => state.setCurrentEvent);

    const handleFileUpload = (e: ChangeEvent<HTMLInputElement>) => {
        const file = e.target.files?.[0];
        if (file) {
            loadEventData(file).then((loadedEvents) => {
                setEvents(loadedEvents);
                const eventNumbers = Object.keys(loadedEvents).map(Number);
                const firstEventNumber = eventNumbers[0];
                setCurrentEventNumber(firstEventNumber);
                setCurrentEvent(loadedEvents[firstEventNumber]);
            });
        }
    };

    return (
        <Button
            variant="contained"
            component="label"
            startIcon={<UploadFileIcon />}
            style={{ marginRight: '8px' }}
        >
            Upload Data File
            <input
                type="file"
                hidden
                accept=".txt"
                onChange={handleFileUpload}
            />
        </Button>
    );
};

export default FileUploader;
