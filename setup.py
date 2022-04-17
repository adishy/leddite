from setuptools import setup

setup(
    name='led_grid',
    version='0.1.0',
    packages=['led_grid'],
    include_package_data=True,
    entry_points={
        'console_scripts': [
            'main=led_grid:main'
        ],
    },
    install_requires=[
        'imageio',
        'arrow==0.15.5',
        'bs4==0.0.1',
        'Flask',
        'requests==2.22.0',
        'sh==1.12.14',
        'rich',
        "rpi_ws281x;platform_system=='Linux'"
    ],
)
