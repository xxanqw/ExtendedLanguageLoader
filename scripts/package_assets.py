#!/usr/bin/env python3
"""Optional private test packs. Never included in the default/public release."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from zipfile import ZipFile, ZIP_DEFLATED
from prepare_languages import prepare


def package(source, fonts, output):
    source, fonts, output = Path(source).resolve(), Path(fonts).resolve(), Path(output).resolve()
    # Refuse overlap with inputs: cleanup is restricted to our output directory.
    for original in (source, fonts):
        if output == original or output in original.parents or original in output.parents:
            raise ValueError('Private output must be isolated from language/font inputs')
    repo = Path(__file__).resolve().parents[1]
    prepare(source, output / 'text')
    profiles = ('cyrillic', 'ukrainian', 'belarusian')
    font_files = []
    for profile in profiles:
        for name in ('models/fonts.txd', 'data/fonts.dat', 'README.md'):
            original = fonts / profile / name
            target = output / 'language-fonts' / profile / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(original, target)
            font_files.append(target)
    shutil.copyfile(repo / 'dist/ExtendedLanguageLoader.asi', output / 'ExtendedLanguageLoader.asi')
    for original, name in ((repo / 'README.md', 'README.md'), (repo / 'docs/UPGRADING.md', 'UPGRADING.md')):
        shutil.copyfile(original, output / name)
    report_path = output / 'language-build-report.json'
    report = json.loads(report_path.read_text())
    report['distribution'] = 'private assets; redistribution permissions must be verified separately'
    report['font_assets'] = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in font_files if p.suffix in ('.txd', '.dat')}
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    base = [output / p for p in ('ExtendedLanguageLoader.asi', 'README.md', 'UPGRADING.md', 'language-build-report.json', 'text/languages.ini')]
    def archive(name, paths):
        with ZipFile(output / name, 'w', ZIP_DEFLATED) as z:
            for path in paths: z.write(path, path.relative_to(output))
    archive('ExtendedLanguageLoader-v0.4.0-private.zip', base + font_files +
            [output / 'text' / language['file'] for language in report['languages']])
    archive('ExtendedLanguageLoader-v0.4.0-private-update.zip', base + font_files)
    print(f'Private pack: {output}; NOT a cleared public release')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('fonts', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    package(args.source, args.fonts, args.output)
