;;; fala-mode.el --- Major mode for Fala  -*- lexical-binding: t; -*-

;; Author: Guilherme Martins Machado <machadog@alunos.utfpr.edu.br>
;; Keywords: languages

;; Package-Version: 0.0.1
;; Package-Requires: ((subr-x) (font-lock) (regexp-opt) (derived))

;;; Commentary:

;;; Code:

(defconst fala-syntax-table
  (let ((table (make-syntax-table)))
    (modify-syntax-entry ?\# "<" table) ;; comment start character
    (modify-syntax-entry ?\n ">" table) ;; comment end character
    (modify-syntax-entry ?\" "\"" table) ;; string delimiter character
    table))

(defconst fala-builtins
  '("array" "read" "write"))

(defconst fala-keywords
  '("if" "then" "else" "when" "while" "for" "from" "to" "step" "var" "do" "end" "let" "in" "and" "or" "not" "fun"))

(defconst fala-constants
  '("nil" "true"))

(defun fala-font-lock-keywords ()
  `(("\\(\\b[0-9]+\\_>\\|\\_<[0-9]+\\b\\)" . font-lock-constant-face) ;; FIXME
    (,(regexp-opt fala-constants 'symbols) . font-lock-constant-face)
    (,(regexp-opt fala-builtins  'symbols) . font-lock-builtin-face)
    (,(regexp-opt fala-keywords  'symbols) . font-lock-keyword-face)))

;;;###autoload
(define-derived-mode fala-mode prog-mode "Fala"
  "Major mode for the Fala programming language."
  :syntax-table fala-syntax-table
  (setq-local font-lock-defaults '(fala-font-lock-keywords))
  (setq-local comment-start "# "))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.fala\\'" . fala-mode))

(provide 'fala-mode)

;;; fala-mode.el ends here
