FILENAME=tasklab

all:
	pdflatex $(FILENAME)
	bibtex $(FILENAME)
	pdflatex $(FILENAME)
	pdflatex $(FILENAME)

clean:
	rm -f *.pdf *.aux *.log *.out