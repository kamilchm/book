(jbuild_version 1)

(alias (
  (name runtest)
  (deps (github_org.sh (files_recursively_in .)))
  (action (
    progn
    (run   cram ${<})
    (diff? ${<} ${<}.corrected)))))

(alias (
  (name runtest-all)
  (deps (github_org.sh (files_recursively_in .)))
  (action (
    progn (run cram --non-deterministic ${<}) (diff? ${<} ${<}.corrected)))))

