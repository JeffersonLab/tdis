"""Unit tests for pytdis.io module."""

import unittest
import tempfile
import os
import numpy as np
import pandas as pd
from pytdis.io import (
    parse_event_data,
    read_tdis_data_numpy,
    read_tdis_data_pandas,
    get_track_summary
)


class TestParseEventData(unittest.TestCase):
    """Test cases for parse_event_data function."""

    def test_parse_valid_event(self):
        """Test parsing a valid event with track and hits."""
        lines = [
            "0.391797\t56.63\t-95.11\t0.0532",
            "1143.44\t7.11161e-07\t0.000319447\t-0.050833\t0.0866874\t0\t91\t6\t0.0366874",
            "1160.56\t9.25686e-08\t0.000404208\t-0.0516643\t0.0872368\t0\t91\t6\t0.0372368"
        ]

        track_info, hits = parse_event_data(lines, event_num=0)

        # Check track info
        self.assertEqual(track_info.shape, (4,))
        self.assertAlmostEqual(track_info[0], 0.391797)
        self.assertAlmostEqual(track_info[1], 56.63)
        self.assertAlmostEqual(track_info[2], -95.11)
        self.assertAlmostEqual(track_info[3], 0.0532)

        # Check hits
        self.assertEqual(len(hits), 2)
        self.assertEqual(hits[0].shape, (9,))
        self.assertAlmostEqual(hits[0][0], 1143.44)
        self.assertEqual(int(hits[0][5]), 0)  # ring
        self.assertEqual(int(hits[0][6]), 91)  # pad
        self.assertEqual(int(hits[0][7]), 6)   # plane

    def test_parse_empty_event(self):
        """Test parsing an empty event raises ValueError."""
        with self.assertRaises(ValueError) as context:
            parse_event_data([], event_num=1)
        self.assertIn("Event 1 has no data", str(context.exception))

    def test_parse_invalid_track_data(self):
        """Test parsing event with invalid track data."""
        lines = ["0.391797\t56.63\t-95.11"]  # Missing z_vertex

        with self.assertRaises(ValueError) as context:
            parse_event_data(lines, event_num=2)
        self.assertIn("Expected 4 track parameters", str(context.exception))

    def test_parse_invalid_hit_data(self):
        """Test parsing event with invalid hit data."""
        lines = [
            "0.391797\t56.63\t-95.11\t0.0532",
            "1143.44\t7.11161e-07\t0.000319447"  # Missing hit parameters
        ]

        with self.assertRaises(ValueError) as context:
            parse_event_data(lines, event_num=3)
        self.assertIn("Expected 9 hit parameters", str(context.exception))


class TestReadTDISData(unittest.TestCase):
    """Test cases for read_tdis_data functions."""

    def setUp(self):
        """Create a temporary test file."""
        self.test_data = """Event 0
\t0.391797\t56.63\t-95.11\t0.0532
\t1143.44\t7.11161e-07\t0.000319447\t-0.050833\t0.0866874\t0\t91\t6\t0.0366874
\t1160.56\t9.25686e-08\t0.000404208\t-0.0516643\t0.0872368\t0\t91\t6\t0.0372368
Event 1
\t0.5123\t45.2\t-120.5\t0.0621
\t1200.0\t1.5e-06\t0.0005\t-0.06\t0.09\t1\t85\t5\t0.04
\t1250.0\t2.1e-06\t0.0006\t-0.065\t0.095\t1\t86\t5\t0.045
\t1300.0\t1.8e-06\t0.0007\t-0.07\t0.1\t2\t87\t5\t0.05
Event 2
\t0.623\t38.7\t-89.3\t0.0712
\t1400.0\t3.2e-06\t0.0008\t-0.075\t0.105\t0\t90\t7\t0.055
"""
        self.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt')
        self.temp_file.write(self.test_data)
        self.temp_file.close()

    def tearDown(self):
        """Remove the temporary test file."""
        os.unlink(self.temp_file.name)

    def test_read_numpy_all_events(self):
        """Test reading all events as numpy arrays."""
        tracks, hits = read_tdis_data_numpy(self.temp_file.name)

        # Check shapes
        self.assertEqual(tracks.shape, (3, 4))  # 3 events, 4 track parameters
        self.assertEqual(hits.shape[0], 3)      # 3 events
        self.assertEqual(hits.shape[2], 9)      # 9 hit parameters

        # Check first track
        self.assertAlmostEqual(tracks[0, 0], 0.391797)
        self.assertAlmostEqual(tracks[0, 1], 56.63)

        # Check that event 1 has 3 hits
        valid_hits_event1 = ~np.isnan(hits[1, :, 0])
        self.assertEqual(np.sum(valid_hits_event1), 3)

    def test_read_numpy_limited_events(self):
        """Test reading limited number of events."""
        tracks, hits = read_tdis_data_numpy(self.temp_file.name, n_events=2)

        self.assertEqual(tracks.shape[0], 2)  # Only 2 events
        self.assertEqual(hits.shape[0], 2)

    def test_read_numpy_skip_events(self):
        """Test skipping events."""
        tracks, hits = read_tdis_data_numpy(self.temp_file.name, skip_events=1)

        self.assertEqual(tracks.shape[0], 2)  # 2 events after skipping 1
        self.assertAlmostEqual(tracks[0, 0], 0.5123)  # First track is now event 1

    def test_read_pandas_all_events(self):
        """Test reading all events as pandas DataFrame."""
        df = read_tdis_data_pandas(self.temp_file.name)

        # Check structure
        self.assertIsInstance(df, pd.DataFrame)
        self.assertEqual(len(df.index.names), 2)
        self.assertEqual(df.index.names, ['track_id', 'hit_id'])

        # Check number of tracks
        n_tracks = df.index.get_level_values('track_id').nunique()
        self.assertEqual(n_tracks, 3)

        # Check columns
        expected_cols = ['momentum', 'theta', 'phi', 'z_vertex',
                         'time', 'adc', 'true_x', 'true_y', 'true_z',
                         'ring', 'pad', 'plane', 'z_to_gem']
        self.assertListEqual(list(df.columns), expected_cols)

        # Check data types
        self.assertEqual(df['ring'].dtype, int)
        self.assertEqual(df['pad'].dtype, int)
        self.assertEqual(df['plane'].dtype, int)

    def test_get_track_summary(self):
        """Test track summary generation."""
        df = read_tdis_data_pandas(self.temp_file.name)
        summary = get_track_summary(df)

        # Check summary structure
        self.assertEqual(len(summary), 3)  # 3 tracks
        self.assertIn('n_hits', summary.columns)
        self.assertIn('momentum', summary.columns)
        self.assertIn('n_rings', summary.columns)

        # Check values for track 1 (which has 3 hits)
        track1_summary = summary.loc[1]
        self.assertEqual(track1_summary['n_hits'], 3)
        self.assertAlmostEqual(track1_summary['momentum'], 0.5123)

    def test_empty_file(self):
        """Test reading an empty file."""
        empty_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
        empty_file.close()

        try:
            tracks, hits = read_tdis_data_numpy(empty_file.name)
            self.assertEqual(tracks.shape[0], 0)
            self.assertEqual(hits.shape, (0, 0, 9))

            df = read_tdis_data_pandas(empty_file.name)
            self.assertTrue(df.empty)
        finally:
            os.unlink(empty_file.name)

    def test_file_not_found(self):
        """Test reading non-existent file."""
        with self.assertRaises(FileNotFoundError):
            read_tdis_data_numpy("non_existent_file.txt")

        with self.assertRaises(FileNotFoundError):
            read_tdis_data_pandas("non_existent_file.txt")


class TestDataConsistency(unittest.TestCase):
    """Test consistency between NumPy and Pandas readers."""

    def setUp(self):
        """Create test data."""
        self.test_data = """Event 0
\t0.5\t60.0\t-90.0\t0.05
\t1000.0\t1e-06\t0.001\t-0.05\t0.08\t0\t90\t5\t0.03
\t1100.0\t2e-06\t0.002\t-0.06\t0.09\t1\t91\t5\t0.04
"""
        self.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt')
        self.temp_file.write(self.test_data)
        self.temp_file.close()

    def tearDown(self):
        """Clean up."""
        os.unlink(self.temp_file.name)

    def test_numpy_pandas_consistency(self):
        """Test that NumPy and Pandas readers give consistent results."""
        tracks_np, hits_np = read_tdis_data_numpy(self.temp_file.name)
        df = read_tdis_data_pandas(self.temp_file.name)

        # Check track info consistency
        track0_df = df.loc[0].iloc[0]  # First hit of first track
        self.assertAlmostEqual(tracks_np[0, 0], track0_df['momentum'])
        self.assertAlmostEqual(tracks_np[0, 1], track0_df['theta'])
        self.assertAlmostEqual(tracks_np[0, 2], track0_df['phi'])
        self.assertAlmostEqual(tracks_np[0, 3], track0_df['z_vertex'])

        # Check hit info consistency
        hit0_0_df = df.loc[(0, 0)]  # First hit of first track
        self.assertAlmostEqual(hits_np[0, 0, 0], hit0_0_df['time'])
        self.assertAlmostEqual(hits_np[0, 0, 1], hit0_0_df['adc'])
        self.assertEqual(int(hits_np[0, 0, 5]), hit0_0_df['ring'])
        self.assertEqual(int(hits_np[0, 0, 6]), hit0_0_df['pad'])
        self.assertEqual(int(hits_np[0, 0, 7]), hit0_0_df['plane'])


if __name__ == '__main__':
    unittest.main()