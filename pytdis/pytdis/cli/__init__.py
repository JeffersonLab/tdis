"""PyTDIS command-line interface."""

import click
from pytdis.version import version
from pytdis.cli.plot import plot_command


@click.group(invoke_without_command=True)
@click.option('--version', '-v', is_flag=True, help='Show version and exit.')
@click.pass_context
def pytdis_cli(ctx, version_flag):
    """
    PyTDIS - Python library for TDIS mTPC detector data analysis.

    Use 'pytdis COMMAND --help' for more information on a command.
    """
    if version_flag:
        click.echo(f"pytdis version {version}")
        ctx.exit()

    # If no subcommand is provided, show help
    if ctx.invoked_subcommand is None:
        click.echo(ctx.get_help())


# Register commands
pytdis_cli.add_command(plot_command)


__all__ = ['pytdis_cli']