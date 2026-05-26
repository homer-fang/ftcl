\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{styles/jouthesis}[2026/05/26 Minimal English thesis class for FTCL]
\LoadClass[12pt,a4paper,openany]{book}

\RequirePackage[a4paper,left=3.0cm,right=2.6cm,top=2.8cm,bottom=2.8cm]{geometry}
\RequirePackage{amsmath,amssymb,amsthm}
\RequirePackage{booktabs,longtable,tabularx,array,multirow}
\RequirePackage{graphicx,float}
\RequirePackage{xcolor}
\RequirePackage{listings}
\RequirePackage{caption}
\RequirePackage{enumitem}
\RequirePackage{fancyhdr}
\RequirePackage{setspace}
\RequirePackage{hyperref}
\RequirePackage[backend=biber,style=numeric,sorting=none]{biblatex}
\addbibresource{contents/references.bib}

\graphicspath{{contents/figures/}{docs/figures/}{docs/figures/architecture/}{docs/figures/geometry/}{docs/figures/benchmarks/}}

\hypersetup{
  colorlinks=true,
  linkcolor=black,
  citecolor=black,
  urlcolor=blue,
  bookmarksopen=true
}

\setstretch{1.30}
\setlength{\parindent}{1.5em}
\setlength{\parskip}{0.20em}
\setlength{\headheight}{14pt}
\captionsetup{font=small,labelfont=bf}
\setlist{nosep,leftmargin=2.0em}

\pagestyle{fancy}
\fancyhf{}
\fancyhead[C]{\small\leftmark}
\fancyfoot[C]{\thepage}
\renewcommand{\headrulewidth}{0.4pt}

\lstdefinestyle{ftclstyle}{
  basicstyle=\ttfamily\small,
  breaklines=true,
  columns=fullflexible,
  frame=single,
  rulecolor=\color{black!25},
  backgroundcolor=\color{black!3},
  keywordstyle=\color{blue!70!black}\bfseries,
  commentstyle=\color{green!40!black},
  stringstyle=\color{orange!60!black}
}
\lstset{style=ftclstyle}

\newcommand{\JOUThesisTitleCN}{A Tcl-style CPU/CUDA Hybrid Computational Geometry Scripting System}
\newcommand{\JOUThesisTitleEN}{A Tcl-style CPU/CUDA Hybrid Computational Geometry Scripting System}
\newcommand{\JOUAuthor}{Fang Hongming}
\newcommand{\JOUStudentID}{To be filled}
\newcommand{\JOUCollege}{To be filled}
\newcommand{\JOUMajor}{Computer Science and Technology}
\newcommand{\JOUAdvisor}{To be filled}
\newcommand{\JOUDate}{May 2026}

\newcommand{\JOUTitleCN}[1]{\gdef\JOUThesisTitleCN{#1}}
\newcommand{\JOUTitleEN}[1]{\gdef\JOUThesisTitleEN{#1}}
\newcommand{\JOUSetAuthor}[1]{\gdef\JOUAuthor{#1}}
\newcommand{\JOUSetStudentID}[1]{\gdef\JOUStudentID{#1}}
\newcommand{\JOUSetCollege}[1]{\gdef\JOUCollege{#1}}
\newcommand{\JOUSetMajor}[1]{\gdef\JOUMajor{#1}}
\newcommand{\JOUSetAdvisor}[1]{\gdef\JOUAdvisor{#1}}
\newcommand{\JOUSetDate}[1]{\gdef\JOUDate{#1}}
\newcommand{\JOUSharedApplyToThesis}{}

\newcommand{\makecover}{%
  \begin{titlepage}
    \centering
    \vspace*{1.0cm}
    {\Large Undergraduate Graduation Thesis\\[1.2cm]}
    {\LARGE\bfseries \JOUThesisTitleEN\\[0.8cm]}
    {\large A system study of FTCL, UVec, and GPU-accelerated geometry\\[2.0cm]}
    \begin{tabular}{>{\bfseries}r@{\quad}l}
      College: & \JOUCollege \\
      Major: & \JOUMajor \\
      Student: & \JOUAuthor \\
      Student ID: & \JOUStudentID \\
      Advisor: & \JOUAdvisor \\
      Date: & \JOUDate \\
    \end{tabular}
    \vfill
    {\large This draft is generated from the FTCL project artifacts and can be refined to match the final university format.}
  \end{titlepage}
  \cleardoublepage
}

\newenvironment{cnabstract}{%
  \chapter*{Abstract}
  \addcontentsline{toc}{chapter}{Abstract}
}{\cleardoublepage}
\newcommand{\cnkeywords}[1]{\par\vspace{1em}\noindent\textbf{Keywords: }#1}

\newenvironment{enabstract}{%
  \chapter*{Extended Abstract}
  \addcontentsline{toc}{chapter}{Extended Abstract}
}{\cleardoublepage}
\newcommand{\enkeywords}[1]{\par\vspace{1em}\noindent\textbf{Keywords: }#1}

\newcommand{\JOUListOfTablesAndFigures}{%
  \listoffigures
  \listoftables
  \cleardoublepage
}

\newcommand{\JOUBackmatterChapter}[1]{\chapter{#1}}
\newcommand{\JOUAcknowledgementChapter}{%
  \chapter*{Acknowledgements}
  \addcontentsline{toc}{chapter}{Acknowledgements}
}
\newcommand{\JOUPrintBibliography}{%
  \cleardoublepage
  \printbibliography[heading=bibintoc,title={References}]
}
