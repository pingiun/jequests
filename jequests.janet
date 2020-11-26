(import cjequests)

(def json
  (try (require "json")
     ([err] ())))

(defn- add-decode-fn
  [tbl]
  (let [decode (get (get json 'decode) :value)]
    (if (= decode nil)
      tbl
      (put tbl :json
         (fn [self]
           (decode (self :text)))))))

(defn- make-kw-key
  [[key value]]
  [(keyword (string/ascii-lower key)) value])

(defn request
  [method url]
  (let [response (cjequests/request method url)
        headers-arr (mapcat (fn [x]
                              (->>
                                (string/split ":" x 0 2)
                                (map string/trim)
                                (make-kw-key)))
                            (response :headers))
        headers (table (splice headers-arr))]
    (add-decode-fn (put response :headers headers-arr))))

(defn get
  "Perform a GET request to the specified url. Adds a method called :json to the
  table if we have the json module available which will try to decode any json
  in the response."
  [url]
  (request :get url))
