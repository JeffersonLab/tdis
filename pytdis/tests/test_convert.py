"""Unit tests for pytdis convert command."""

import unittest
import tempfile
import os
import numpy as np
import pandas as pd
from click.testing import CliRunner
from pytdis.cli import pytdis_cli


class TestConvertCommand(unittest.TestCase):
    """Test cases for the convert CLI command."""

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
"""
        self.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt')
        self.temp_file.write(self.test_data)
        self.temp_file.close()

        self.runner = CliRunner()
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        """Remove temporary files."""
        os.unlink(self.temp_file.name)
        # Clean up any created output files
        for file in os.listdir(self.temp_dir):
            os.unlink(os.path.join(self.temp_dir, file))
        os.rmdir(self.temp_dir)

    def test_convert_to_csv_default(self):
        """Test default conversion to CSV."""
        with self.runner.isolated_filesystem():
            # Copy test file to isolated filesystem
            import shutil
            shutil.copy(self.temp_file.name, 'test_data.txt')

            # Run convert command
            result = self.runner.invoke(pytdis_cli, ['convert', 'test_data.txt'])

            self.assertEqual(result.exit_code, 0)
            self.assertIn('Successfully converted to: test_data.csv', result.output)
            self.assertTrue(os.path.exists('test_data.csv'))

            # Verify CSV content
            df = pd.read_csv('test_data.csv', index_col=[0, 1])
            self.assertEqual(len(df.index.get_level_values('track_id').unique()), 2)
            self.assertAlmostEqual(df.loc[(0, 0), 'momentum'], 0.391797)

    def test_convert_to_csv_custom_output(self):
        """Test conversion to CSV with custom output name."""
        with self.runner.isolated_filesystem():
            import shutil
            shutil.copy(self.temp_file.name, 'test_data.txt')

            result = self.runner.invoke(pytdis_cli,
                                        ['convert', 'test_data.txt', '-o', 'custom.csv'])

            self.assertEqual(result.exit_code, 0)
            self.assertIn('Successfully converted to: custom.csv', result.output)
            self.assertTrue(os.path.exists('custom.csv'))

    def test_convert_to_npz(self):
        """Test conversion to NPZ format."""
        with self.runner.isolated_filesystem():
            import shutil
            shutil.copy(self.temp_file.name, 'test_data.txt')

            result = self.runner.invoke(pytdis_cli,
                                        ['convert', 'test_data.txt', '-o', 'arrays.npz'])

            self.assertEqual(result.exit_code, 0)
            self.assertIn('Successfully converted to: arrays.npz', result.output)
            self.assertTrue(os.path.exists('arrays.npz'))

            # Verify NPZ content
            data = np.load('arrays.npz')
            self.assertIn('tracks', data.files)
            self.assertIn('hits', data.files)
            self.assertEqual(data['tracks'].shape[0], 2)  # 2 events
            self.assertAlmostEqual(data['tracks'][0, 0], 0.391797)  # momentum

    def test_convert_with_event_selection(self):
        """Test conversion with event selection options."""
        with self.runner.isolated_filesystem():
            import shutil
            shutil.copy(self.temp_file.name, 'test_data.txt')

            # Convert only 1 event, skipping the first
            result = self.runner.invoke(pytdis_cli,
                                        ['convert', 'test_data.txt', '-n', '1', '-s', '1',
                                         '-o', 'subset.csv'])

            self.assertEqual(result.exit_code, 0)
            self.assertTrue(os.path.exists('subset.csv'))

            # Verify only one event in output
            df = pd.read_csv('subset.csv', index_col=[0, 1])
            n_tracks = len(df.index.get_level_values('track_id').unique())
            self.assertEqual(n_tracks, 1)
            # Should be the second event (first was skipped)
            self.assertAlmostEqual(df.iloc[0]['momentum'], 0.5123)

    def test_convert_verbose_mode(self):
        """Test verbose output."""
        with self.runner.isolated_filesystem():
            import shutil
            shutil.copy(self.temp_file.name, 'test_data.txt')

            result = self.runner.invoke(pytdis_cli,
                                        ['convert', 'test_data.txt', '-v'])

            self.assertEqual(result.exit_code, 0)
            self.assertIn('Reading TDIS data from:', result.output)
            self.assertIn('Converting to pandas DataFrame format', result.output)
            self.assertIn('Read 2 tracks', result.output)
            self.assertIn('DataFrame shape:', result.output)

    def test_convert_nonexistent_file(self):
        """Test error handling for non-existent file."""
        result = self.runner.invoke(pytdis_cli,
                                    ['convert', 'nonexistent.txt'])

        self.assertNotEqual(result.exit_code, 0)
        self.assertIn("Error: File 'nonexistent.txt' not found", result.output)

    def test_convert_data_types(self):
        """Test that data types are preserved correctly."""
        with self.runner.isolated_filesystem():
            import shutil
            shutil.copy(self.temp_file.name, 'test_data.txt')

            # Test CSV output
            result = self.runner.invoke(pytdis_cli,
                                        ['convert', 'test_data.txt', '-o', 'types.csv'])
            self.assertEqual(result.exit_code, 0)

            df = pd.read_csv('types.csv', index_col=[0, 1])
            # Check that ring, pad, plane are integers
            self.assertEqual(df['ring'].dtype, np.int64)
            self.assertEqual(df['pad'].dtype, np.int64)
            self.assertEqual(df['plane'].dtype, np.int64)

            # Test NPZ output
            result = self.runner.invoke(pytdis_cli,
                                        ['convert', 'test_data.txt', '-o', 'types.npz'])
            self.assertEqual(result.exit_code, 0)

            data = np.load('types.npz')
            hits = data['hits']
            # Check that ring, pad, plane values are integers (stored as floats in array)
            self.assertEqual(int(hits[0, 0, 5]), 0)  # ring
            self.assertEqual(int(hits[0, 0, 6]), 91)  # pad
            self.assertEqual(int(hits[0, 0, 7]), 6)   # plane


if __name__ == '__main__':
    unittest.main()