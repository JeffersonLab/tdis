// src/utils/DataLoader.ts
export interface EventData {
    trackParams: number[];
    hits: number[][];
}

export async function loadEventData(file: File | string): Promise<{ [key: number]: EventData }> {
    let text: string;

    if (typeof file === 'string') {
        // Fetch from URL
        const response = await fetch(process.env.PUBLIC_URL + '/' + file);
        text = await response.text();
    } else {
        // Read from uploaded file
        text = await file.text();
    }

    return parseEvents(text);
}

function parseEvents(data: string): { [key: number]: EventData } {
    const lines = data.split('\n');
    const events: { [key: number]: EventData } = {};
    let i = 0;
    while (i < lines.length) {
        const line = lines[i].trim();
        if (line.startsWith('Event')) {
            const eventNumber = parseInt(line.split(' ')[1], 10);
            i++;
            if (i >= lines.length) break;
            const trackParams = lines[i].trim().split(/\s+/).map(Number);
            i++;
            const hits: number[][] = [];
            while (i < lines.length && !lines[i].trim().startsWith('Event')) {
                const hitLine = lines[i].trim();
                if (hitLine) {
                    const hitData = hitLine.split(/\s+/).map(Number);
                    hits.push(hitData);
                }
                i++;
            }
            events[eventNumber] = { trackParams, hits };
        } else {
            i++;
        }
    }
    return events;
}

export async function loadRawData(filename: string): Promise<string> {
    const response = await fetch(process.env.PUBLIC_URL + '/' + filename);
    const text = await response.text();
    return text;
}
