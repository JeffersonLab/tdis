// src/components/RawDataDisplay.tsx
import React, { useEffect, useState } from 'react';
import { Box, Typography, Button } from '@mui/material';
import { Link } from 'react-router-dom';
import { loadRawData } from '../utils/DataLoader';

const RawDataDisplay: React.FC = () => {
    const [rawData, setRawData] = useState<string>('');

    useEffect(() => {
        loadRawData('track_data.txt').then((data) => {
            setRawData(data);
        });
    }, []);

    return (
        <Box p={2}>
            <Typography variant="h4" gutterBottom>
                Raw Data Display
            </Typography>
            <Button
                variant="contained"
                component={Link}
                to="/"
                style={{ marginBottom: '16px' }}
            >
                Back to Tracking Display
            </Button>
            <Box
                component="pre"
                p={2}
                sx={{
                    backgroundColor: '#f5f5f5',
                    borderRadius: '4px',
                    overflow: 'auto',
                    maxHeight: '600px',
                }}
            >
                {rawData}
            </Box>
        </Box>
    );
};

export default RawDataDisplay;
