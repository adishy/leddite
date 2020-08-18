from setuptools import setup

setup(
    name='led_grid',
    version='0.1.0',
    packages=['led_grid'],
    include_package_data=True,
    install_requires=[
        'arrow==0.15.5',
        'bs4==0.0.1',
        'Flask==1.1.1',
        'requests==2.22.0',
        'sh==1.12.14',
        'rpi_ws281x'
    ],
)
