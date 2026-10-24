project = catalan_admin

type:
	poetry run python -m mypy --strict $(project)/ test/

lint: type
	poetry run python -m flake8 $(project)/ test/

format:
	poetry run python -m black $(project)/ test/

test: lint
	poetry run pytest --cov-branch --cov=$(project) --disable-warnings -s
