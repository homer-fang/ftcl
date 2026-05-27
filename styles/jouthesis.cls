\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{styles/jouthesis}[2026/05/26 Minimal English thesis class for FTCL]
\LoadClass[12pt,a4paper,openany]{book}

\RequirePackage[a4paper,left=3.0cm,right=2.6cm,top=2.8cm,bottom=2.8cm]{geometry}
\RequirePackage{amsmath,amssymb,amsthm}
\RequirePackage{booktabs,longtable,tabularx,array,multirow}
\RequirePackage{graphicx,float}
\RequirePackage[ruled,vlined,linesnumbered]{algorithm2e}
\RequirePackage{xcolor}
\RequirePackage{listings}
\RequirePackage{caption}
\RequirePackage{enumitem}
\RequirePackage{fancyhdr}
\RequirePackage{setspace}
\RequirePackage{fontspec}
\RequirePackage{xeCJK}
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
\setmainfont{Latin Modern Roman}
\setsansfont{Latin Modern Sans}
\setmonofont{Latin Modern Mono}
\IfFontExistsTF{AR PL UMing CN}{
  \setCJKmainfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{AR PL UMing CN}
  \setCJKsansfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{AR PL UMing CN}
}{
  \setCJKmainfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{FandolSong-Regular}
  \setCJKsansfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{FandolHei-Regular}
}
\IfFontExistsTF{TeX Gyre Termes}{
  \newfontfamily\JOUCoverEnglishFont{TeX Gyre Termes}
}{
  \newfontfamily\JOUCoverEnglishFont{Latin Modern Roman}
}

\SetAlgoCaptionSeparator{:}
\SetAlFnt{\small}
\SetAlgoNlRelativeSize{-1}
\SetAlgoInsideSkip{smallskip}
\SetKwInput{KwInput}{Input}
\SetKwInput{KwData}{Data}
\SetKwInput{KwOutput}{Output}
\SetKw{KwReturn}{return}
\SetKwProg{Fn}{Function}{:}{end}
\SetKwProg{Proc}{Procedure}{:}{end}

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

\newlength{\JOUCoverLabelWidth}
\setlength{\JOUCoverLabelWidth}{3.8cm}
\newlength{\JOUCoverValueWidth}
\setlength{\JOUCoverValueWidth}{8.4cm}
\newlength{\JOUCoverShortValueWidth}
\setlength{\JOUCoverShortValueWidth}{2.9cm}
% -----------------------------------------------------------------------------
% Cover layout parameters
% -----------------------------------------------------------------------------

\newcommand{\JOUCoverHeaderImage}{jou-cover-header.png}

% 整个封面内容距离页面顶部的距离。
% 调整它会带动校徽、主标题、题目等整体上下移动。
\newcommand{\JOUCoverHeaderTopVskip}{0.85cm}

% 仅调整校徽图片的视觉位置。
% 正值表示上移，负值表示下移；不会影响下方标题的位置。
\newcommand{\JOUCoverHeaderVisualRaise}{15mm}

% 校徽图片与“本科毕业设计（论文）”之间的版面间距。
\newcommand{\JOUCoverMainTitleVskip}{0.22cm}

% “本科毕业设计（论文）”这一行自身的微调。
% 正值表示上移，负值表示下移。
\newcommand{\JOUCoverMainTitleRaise}{0.15ex}

% “本科毕业设计（论文）”与“毕业设计（论文）中文题目”之间的距离。
\newcommand{\JOUCoverSubtitleVskip}{3.45cm}

\newcommand{\JOUCoverIncludeHeaderImage}{%
  \includegraphics[
    width=12.0cm,
    trim=35 300 35 175,
    clip
  ]{\JOUCoverHeaderImage}%
}

% 放置校徽图片。
% 关键点：[\height][\depth] 保留原盒子高度，只改变图片视觉位置。
\newcommand{\JOUCoverPlaceHeaderImage}{%
  \raisebox{\JOUCoverHeaderVisualRaise}[\height][\depth]{%
    \JOUCoverIncludeHeaderImage
  }%
}

\newcommand{\JOUCoverCJKBold}[1]{{\sffamily\bfseries #1}}
\newcommand{\JOUCoverUnderline}[2]{%
  \makebox[#1][c]{#2}\leaders\hrule height 0.42pt\hfill\kern0pt%
}
\newcommand{\JOUCoverLine}[2]{%
  \noindent
  \makebox[\JOUCoverLabelWidth][s]{\JOUCoverCJKBold{#1}}\quad
  \makebox[\JOUCoverValueWidth][l]{%
    \leaders\hrule height 0.42pt\hfill
    \llap{\makebox[\JOUCoverValueWidth][c]{#2}}%
  }\par
}
\newcommand{\JOUCoverStudentLine}{%
  \noindent
  \makebox[\JOUCoverLabelWidth][s]{\JOUCoverCJKBold{学生姓名：}}\quad
  \makebox[3.4cm][l]{%
    \leaders\hrule height 0.42pt\hfill
    \llap{\makebox[3.4cm][c]{\JOUAuthor}}%
  }%
  \hspace{0.45cm}%
  \makebox[1.35cm][s]{\JOUCoverCJKBold{学号：}}%
  \makebox[3.0cm][l]{%
    \leaders\hrule height 0.42pt\hfill
    \llap{\makebox[3.0cm][c]{\JOUStudentID}}%
  }\par
}

\newcommand{\makecover}{%
  \begin{titlepage}
    \thispagestyle{empty}
    \newgeometry{left=2.6cm,right=2.6cm,top=1.55cm,bottom=1.8cm}
    \centering
    \vspace*{\JOUCoverHeaderTopVskip}

    {\JOUCoverPlaceHeaderImage}\par

    \vspace{\JOUCoverMainTitleVskip}
    {\raisebox{\JOUCoverMainTitleRaise}{%
      \sffamily\bfseries\fontsize{24pt}{32pt}\selectfont
      本\quad 科\quad 毕\quad 业\quad 设\quad 计\quad （论\quad 文）%
    }\par}
    \vspace{\JOUCoverSubtitleVskip}
    {\sffamily\bfseries\fontsize{18pt}{28pt}\selectfont 毕业设计（论文）中文题目\par}
    \vspace{0.25cm}
    \begin{minipage}{0.88\textwidth}
      \centering
      {\sffamily\bfseries\fontsize{16pt}{24pt}\selectfont \JOUThesisTitleCN\par}
    \end{minipage}\par
    \vspace{0.52cm}
    {\JOUCoverEnglishFont\bfseries\fontsize{22pt}{28pt}\selectfont English Title\par}
    \vspace{0.22cm}
    \begin{minipage}{0.88\textwidth}
      \centering
      {\JOUCoverEnglishFont\bfseries\fontsize{16pt}{22pt}\selectfont \JOUThesisTitleEN\par}
    \end{minipage}\par
    \vfill
    \begin{minipage}{12.7cm}
      \fontsize{14pt}{25pt}\selectfont
      \JOUCoverLine{学院：}{\JOUCollege}
      \vspace{0.18cm}
      \JOUCoverLine{专业班级：}{\JOUMajor}
      \vspace{0.18cm}
      \JOUCoverStudentLine
      \vspace{0.18cm}
      \JOUCoverLine{指导教师：}{\JOUAdvisor}
    \end{minipage}\par
    \vspace{1.02cm}
    {\sffamily\bfseries\fontsize{16pt}{24pt}\selectfont 2026 年\qquad 5 月\par}
    \restoregeometry
  \end{titlepage}
  \cleardoublepage
}

\newenvironment{cnabstract}{%
  \chapter*{摘要}
  \addcontentsline{toc}{chapter}{摘要}
}{\cleardoublepage}
\newcommand{\cnkeywords}[1]{\par\vspace{1em}\noindent\textbf{关键字：}#1}

\newenvironment{enabstract}{%
  \chapter*{Abstract}
  \addcontentsline{toc}{chapter}{Abstract}
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
