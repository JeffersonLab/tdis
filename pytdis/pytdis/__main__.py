"""Main entry point for pytdis when run as a module."""

from pytdis.cli import pytdis_cli


def main():
    """
    Main entry point if users run `python -m pytdis`
    or if you have a setup entry point referencing `pytdis.__main__:main`.
    """
    pytdis_cli()


if __name__ == '__main__':
    main()